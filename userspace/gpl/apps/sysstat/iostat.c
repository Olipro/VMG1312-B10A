/*
 * iostat: report CPU and I/O statistics
 * (C) 1998-2009 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                   *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "version.h"
#include "iostat.h"
#include "common.h"
#include "ioconf.h"
#include "rd_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }

struct stats_cpu *st_cpu[2];
unsigned long long uptime[2]  = {0, 0};
unsigned long long uptime0[2] = {0, 0};
struct io_stats *st_iodev[2];
struct io_nfs_stats *st_ionfs[2];
struct io_hdr_stats *st_hdr_iodev, *st_hdr_ionfs;
struct io_dlist *st_dev_list;

int iodev_nr = 0;	/* Nb of devices and partitions found */
int ionfs_nr = 0;	/* Nb of NFS mounted directories found */
int cpu_nr = 0;		/* Nb of processors on the machine */
int dlist_idx = 0;	/* Nb of devices entered on the command line */
int flags = 0;		/* Flag for common options and system state */

long interval = 0;
char timestamp[64];


/*
 ***************************************************************************
 * Print usage and exit.
 *
 * IN:
 * @progname	Name of sysstat command.
 ***************************************************************************
 */
void usage(char *progname)
{
	fprintf(stderr, _("Usage: %s [ options ] [ <interval> [ <count> ] ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -c ] [ -d ] [ -N ] [ -n ] [ -h ] [ -k | -m ] [ -t ] [ -V ] [ -x ] [ -z ]\n"
			  "[ <device> [...] | ALL ] [ -p [ <device> [,...] | ALL ] ]\n"));
	exit(1);
}

/*
 ***************************************************************************
 * SIGALRM signal handler.
 *
 * IN:
 * @sig	Signal number. Set to 0 for the first time, then to SIGALRM.
 ***************************************************************************
 */
void alarm_handler(int sig)
{
	signal(SIGALRM, alarm_handler);
	alarm(interval);
}

/*
 ***************************************************************************
 * Initialize stats common structures.
 ***************************************************************************
 */
void init_stats(void)
{
	int i;
	
	/* Allocate structures for CPUs "all" and 0 */
	for (i = 0; i < 2; i++) {
		if ((st_cpu[i] = (struct stats_cpu *) malloc(STATS_CPU_SIZE * 2)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_cpu[i], 0, STATS_CPU_SIZE * 2);
	}
}

/*
 ***************************************************************************
 * Set every disk_io or nfs_io entry to inactive state (unregistered).
 *
 * IN:
 * @ioln_nr	Number of devices and partitions or NFS filesystems.
 * @st_hdr_ioln	Pointer on first structure describing a device/partition or
 * 		an NFS filesystem.
 ***************************************************************************
 */
void set_entries_inactive(int ioln_nr, struct io_hdr_stats *st_hdr_ioln)
{
	int i;
	struct io_hdr_stats *shi = st_hdr_ioln;

	for (i = 0; i < ioln_nr; i++, shi++) {
		shi->active = FALSE;
	}
}

/*
 ***************************************************************************
 * Free inactive entries (mark them as unused).
 *
 * IN:
 * @ioln_nr	Number of devices and partitions or NFS filesystems.
 * @st_hdr_ioln	Pointer on first structure describing a device/partition or
 * 		an NFS filesystem.
 ***************************************************************************
 */
void free_inactive_entries(int ioln_nr, struct io_hdr_stats *st_hdr_ioln)
{
	int i;
	struct io_hdr_stats *shi = st_hdr_ioln;

	for (i = 0; i < ioln_nr; i++, shi++) {
		if (!shi->active) {
			shi->used = FALSE;
		}
	}
}

/*
 ***************************************************************************
 * Allocate and init I/O device structures.
 *
 * IN:
 * @iodev_nr	Number of devices and partitions.
 ***************************************************************************
 */
void salloc_device(int iodev_nr)
{
	int i;

	for (i = 0; i < 2; i++) {
		if ((st_iodev[i] =
		     (struct io_stats *) malloc(IO_STATS_SIZE * iodev_nr)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_iodev[i], 0, IO_STATS_SIZE * iodev_nr);
	}

	if ((st_hdr_iodev =
	     (struct io_hdr_stats *) malloc(IO_HDR_STATS_SIZE * iodev_nr)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(st_hdr_iodev, 0, IO_HDR_STATS_SIZE * iodev_nr);
}

/*
 ***************************************************************************
 * Allocate and init I/O NFS directories structures.
 *
 * IN:
 * @ionfs_nr	Number of NFS filesystems.
 ***************************************************************************
 */
void salloc_nfs(int ionfs_nr)
{
	int i;

	for (i = 0; i < 2; i++) {
		if ((st_ionfs[i] =
		     (struct io_nfs_stats *) malloc(IO_NFS_STATS_SIZE * ionfs_nr)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_ionfs[i], 0, IO_NFS_STATS_SIZE * ionfs_nr);
	}

	if ((st_hdr_ionfs =
	     (struct io_hdr_stats *) malloc(IO_HDR_STATS_SIZE * ionfs_nr)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(st_hdr_ionfs, 0, IO_HDR_STATS_SIZE * ionfs_nr);
}

/*
 ***************************************************************************
 * Allocate structures for devices entered on the command line.
 *
 * IN:
 * @list_len	Number of arguments on the command line.
 ***************************************************************************
 */
void salloc_dev_list(int list_len)
{
	if ((st_dev_list = (struct io_dlist *) malloc(IO_DLIST_SIZE * list_len)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(st_dev_list, 0, IO_DLIST_SIZE * list_len);
}

/*
 ***************************************************************************
 * Free structures used for devices entered on the command line.
 ***************************************************************************
 */
void sfree_dev_list(void)
{
	if (st_dev_list) {
		free(st_dev_list);
	}
}

/*
 ***************************************************************************
 * Look for the device in the device list and store it if necessary.
 *
 * IN:
 * @dlist_idx	Length of the device list.
 * @device_name	Name of the device.
 *
 * OUT:
 * @dlist_idx	Length of the device list.
 *
 * RETURNS:
 * Position of the device in the list.
 ***************************************************************************
 */
int update_dev_list(int *dlist_idx, char *device_name)
{
	int i;
	struct io_dlist *sdli = st_dev_list;

	for (i = 0; i < *dlist_idx; i++, sdli++) {
		if (!strcmp(sdli->dev_name, device_name))
			break;
	}

	if (i == *dlist_idx) {
		/* Device not found: Store it */
		(*dlist_idx)++;
		strncpy(sdli->dev_name, device_name, MAX_NAME_LEN - 1);
	}

	return i;
}

/*
 ***************************************************************************
 * Allocate and init structures, according to system state.
 ***************************************************************************
 */
void io_sys_init(void)
{
	int i;

	/* Allocate and init stat common counters */
	init_stats();

	/* How many processors on this machine? */
	cpu_nr = get_cpu_nr(~0);

	/* Get number of block devices and partitions in /proc/diskstats */
	if ((iodev_nr = get_diskstats_dev_nr(CNT_PART, CNT_ALL_DEV)) > 0) {
		flags |= I_F_HAS_DISKSTATS;
		iodev_nr += NR_DEV_PREALLOC;
	}

	if (!HAS_DISKSTATS(flags) ||
	    (DISPLAY_PARTITIONS(flags) && !DISPLAY_PART_ALL(flags))) {
		/*
		 * If /proc/diskstats exists but we also want stats for the partitions
		 * of a particular device, stats will have to be found in /sys. So we
		 * need to know if /sys is mounted or not, and set flags accordingly.
		 */

		/* Get number of block devices (and partitions) in sysfs */
		if ((iodev_nr = get_sysfs_dev_nr(DISPLAY_PARTITIONS(flags))) > 0) {
			flags |= I_F_HAS_SYSFS;
			iodev_nr += NR_DEV_PREALLOC;
		}
		/*
		 * Get number of block devices and partitions in /proc/partitions,
		 * those with statistics...
		 */
		else if ((iodev_nr = get_ppartitions_dev_nr(CNT_PART)) > 0) {
			flags |= I_F_HAS_PPARTITIONS;
			iodev_nr += NR_DEV_PREALLOC;
		}
		/* Get number of "disk_io:" entries in /proc/stat */
		else if ((iodev_nr = get_disk_io_nr()) > 0) {
			flags |= I_F_PLAIN_KERNEL24;
			iodev_nr += NR_DISK_PREALLOC;
		}
		else {
			/* Assume we have an old kernel: stats for 4 disks are in /proc/stat */
			iodev_nr = 4;
			flags |= I_F_OLD_KERNEL;
		}
	}
	/*
	 * Allocate structures for number of disks found.
	 * iodev_nr must be <> 0.
	 */
	salloc_device(iodev_nr);

	if (HAS_OLD_KERNEL(flags)) {
		struct io_hdr_stats *shi = st_hdr_iodev;
		/*
		 * If we have an old kernel with the stats for the first four disks
		 * in /proc/stat, then set the devices names to hdisk[0..3].
		 */
		for (i = 0; i < 4; i++, shi++) {
			shi->used = TRUE;
			sprintf(shi->name, "%s%d", K_HDISK, i);
		}
	}

	/* Get number of NFS directories in /proc/self/mountstats */
	if (DISPLAY_NFS(flags) &&
	    ((ionfs_nr = get_nfs_mount_nr()) > 0)) {
		flags |= I_F_HAS_NFS;
		ionfs_nr += NR_NFS_PREALLOC;

		/* Allocate structures for number of NFS directories found */
		salloc_nfs(ionfs_nr);
	}
}

/*
 ***************************************************************************
 * Free various structures.
 ***************************************************************************
*/
void io_sys_free(void)
{
	int i;
	
	for (i = 0; i < 2; i++) {

		/* Free CPU structures */
		if (st_cpu[i]) {
			free(st_cpu[i]);
		}

		/* Free I/O device structures */
		if (st_iodev[i]) {
			free(st_iodev[i]);
		}
		
		/* Free I/O NFS directories structures */
		if (st_ionfs[i]) {
			free(st_ionfs[i]);
		}
	}
	
	if (st_hdr_iodev) {
		free(st_hdr_iodev);
	}
	if (st_hdr_ionfs) {
		free(st_hdr_ionfs);
	}
}

/*
 ***************************************************************************
 * Save stats for current device, partition or NFS filesystem.
 *
 * IN:
 * @name	Name of the device/partition or NFS filesystem.
 * @curr	Index in array for current sample statistics.
 * @st_io	Structure with device, partition or NFS statistics to save.
 * @ioln_nr	Number of devices and partitions or NFS filesystems.
 * @st_hdr_ioln	Pointer on structures describing a device/partition or an
 *		NFS filesystem.
 *
 * OUT:
 * @st_hdr_ioln	Pointer on structures describing a device/partition or an
 *		NFS filesystem.
 ***************************************************************************
 */
void save_stats(char *name, int curr, void *st_io, int ioln_nr,
		struct io_hdr_stats *st_hdr_ioln)
{
	int i;
	struct io_hdr_stats *st_hdr_ioln_i;
	struct io_stats *st_iodev_i;
	struct io_nfs_stats *st_ionfs_i;

	/* Look for device or NFS directory in data table */
	for (i = 0; i < ioln_nr; i++) {
		st_hdr_ioln_i = st_hdr_ioln + i;
		if (!strcmp(st_hdr_ioln_i->name, name)) {
			break;
		}
	}
	
	if (i == ioln_nr) {
		/*
		 * This is a new device: look for an unused entry to store it.
		 * Thus we are able to handle dynamically registered devices.
		 */
		for (i = 0; i < ioln_nr; i++) {
			st_hdr_ioln_i = st_hdr_ioln + i;
			if (!st_hdr_ioln_i->used) {
				/* Unused entry found... */
				st_hdr_ioln_i->used = TRUE; /* Indicate it is now used */
				strcpy(st_hdr_ioln_i->name, name);
				if (st_hdr_ioln == st_hdr_iodev) {
					st_iodev_i = st_iodev[!curr] + i;
					memset(st_iodev_i, 0, IO_STATS_SIZE);
				}
				else {
					st_ionfs_i = st_ionfs[!curr] + i;
					memset(st_ionfs_i, 0, IO_NFS_STATS_SIZE);
				}
				break;
			}
		}
	}
	if (i < ioln_nr) {
		st_hdr_ioln_i = st_hdr_ioln + i;
		st_hdr_ioln_i->active = TRUE;
		if (st_hdr_ioln == st_hdr_iodev) {
			st_iodev_i = st_iodev[curr] + i;
			*st_iodev_i = *((struct io_stats *) st_io);
		}
		else {
			st_ionfs_i = st_ionfs[curr] + i;
			*st_ionfs_i = *((struct io_nfs_stats *) st_io);
		}
	}
	/*
	 * else it was a new device or NFS directory
	 * but there was no free structure to store it.
	 */
}

/*
 ***************************************************************************
 * Read stats from /proc/stat file...
 * Used to get disk stats if /sys not available.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_proc_stat(int curr)
{
	FILE *fp;
	char line[8192];
	int pos, i;
	unsigned long v_tmp[4];
	unsigned int v_major, v_index;
	struct io_stats *st_iodev_tmp[4];

	/*
	 * Prepare pointers on the 4 disk structures in case we have a
	 * /proc/stat file with "disk_rblk", etc. entries.
	 */
	for (i = 0; i < 4; i++) {
		st_iodev_tmp[i] = st_iodev[curr] + i;
	}

	if ((fp = fopen(STAT, "r")) == NULL) {
		fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
		exit(2);
	}

	while (fgets(line, 8192, fp) != NULL) {

		if (!strncmp(line, "disk_rblk ", 10)) {
			/*
			 * Read the number of blocks read from disk.
			 * A block is of indeterminate size.
			 * The size may vary depending on the device type.
			 */
			sscanf(line + 10, "%lu %lu %lu %lu",
			       &v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);

			st_iodev_tmp[0]->dk_drive_rblk = v_tmp[0];
			st_iodev_tmp[1]->dk_drive_rblk = v_tmp[1];
			st_iodev_tmp[2]->dk_drive_rblk = v_tmp[2];
			st_iodev_tmp[3]->dk_drive_rblk = v_tmp[3];
		}

		else if (!strncmp(line, "disk_wblk ", 10)) {
			/* Read the number of blocks written to disk */
			sscanf(line + 10, "%lu %lu %lu %lu",
			       &v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
	
			st_iodev_tmp[0]->dk_drive_wblk = v_tmp[0];
			st_iodev_tmp[1]->dk_drive_wblk = v_tmp[1];
			st_iodev_tmp[2]->dk_drive_wblk = v_tmp[2];
			st_iodev_tmp[3]->dk_drive_wblk = v_tmp[3];
		}

		else if (!strncmp(line, "disk ", 5)) {
			/* Read the number of I/O done since the last reboot */
			sscanf(line + 5, "%lu %lu %lu %lu",
			       &v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
			
			st_iodev_tmp[0]->dk_drive = v_tmp[0];
			st_iodev_tmp[1]->dk_drive = v_tmp[1];
			st_iodev_tmp[2]->dk_drive = v_tmp[2];
			st_iodev_tmp[3]->dk_drive = v_tmp[3];
		}

		else if (!strncmp(line, "disk_io: ", 9)) {
			struct io_stats sdev;
			char dev_name[MAX_NAME_LEN];

			pos = 9;

			/* Every disk_io entry is potentially unregistered */
			set_entries_inactive(iodev_nr, st_hdr_iodev);
	
			/* Read disks I/O statistics (for 2.4 kernels) */
			while (pos < strlen(line) - 1) {
				/* Beware: a CR is already included in the line */
				sscanf(line + pos, "(%u,%u):(%lu,%*u,%lu,%*u,%lu) ",
				       &v_major, &v_index, &v_tmp[0], &v_tmp[1], &v_tmp[2]);

				sprintf(dev_name, "dev%d-%d", v_major, v_index);
				sdev.dk_drive      = v_tmp[0];
				sdev.dk_drive_rblk = v_tmp[1];
				sdev.dk_drive_wblk = v_tmp[2];
				save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);

				pos += strcspn(line + pos, " ") + 1;
			}

			/* Free structures corresponding to unregistered disks */
			free_inactive_entries(iodev_nr, st_hdr_iodev);
		}
	}

	fclose(fp);
}

/*
 ***************************************************************************
 * Read sysfs stat for current block device or partition.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @filename	File name where stats will be read.
 * @dev_name	Device or partition name.
 *
 * RETURNS:
 * 0 if file couldn't be opened, 1 otherwise.
 ***************************************************************************
 */
int read_sysfs_file_stat(int curr, char *filename, char *dev_name)
{
	FILE *fp;
	struct io_stats sdev;
	int i;
	unsigned long rd_ios, rd_merges_or_rd_sec, rd_ticks_or_wr_sec, wr_ios;
	unsigned long ios_pgr, tot_ticks, rq_ticks, wr_merges, wr_ticks;
	unsigned long long rd_sec_or_wr_ios, wr_sec;

	/* Try to read given stat file */
	if ((fp = fopen(filename, "r")) == NULL)
		return 0;
	
	i = fscanf(fp, "%lu %lu %llu %lu %lu %lu %llu %lu %lu %lu %lu",
		   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
		   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks);

	if (i == 11) {
		/* Device or partition */
		sdev.rd_ios     = rd_ios;
		sdev.rd_merges  = rd_merges_or_rd_sec;
		sdev.rd_sectors = rd_sec_or_wr_ios;
		sdev.rd_ticks   = rd_ticks_or_wr_sec;
		sdev.wr_ios     = wr_ios;
		sdev.wr_merges  = wr_merges;
		sdev.wr_sectors = wr_sec;
		sdev.wr_ticks   = wr_ticks;
		sdev.ios_pgr    = ios_pgr;
		sdev.tot_ticks  = tot_ticks;
		sdev.rq_ticks   = rq_ticks;
	}
	else if (i == 4) {
		/* Partition without extended statistics */
		sdev.rd_ios     = rd_ios;
		sdev.rd_sectors = rd_merges_or_rd_sec;
		sdev.wr_ios     = rd_sec_or_wr_ios;
		sdev.wr_sectors = rd_ticks_or_wr_sec;
	}
	
	if ((i == 11) || !DISPLAY_EXTENDED(flags)) {
		/*
		 * In fact, we _don't_ save stats if it's a partition without
		 * extended stats and yet we want to display ext stats.
		 */
		save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);
	}

	fclose(fp);

	return 1;
}

/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a device.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dev_name	Device name.
 ***************************************************************************
 */
void read_sysfs_dlist_part_stat(int curr, char *dev_name)
{
	DIR *dir;
	struct dirent *drd;
	char dfile[MAX_PF_NAME], filename[MAX_PF_NAME];

	snprintf(dfile, MAX_PF_NAME, "%s/%s", SYSFS_BLOCK, dev_name);
	dfile[MAX_PF_NAME - 1] = '\0';

	/* Open current device directory in /sys/block */
	if ((dir = opendir(dfile)) == NULL)
		return;

	/* Get current entry */
	while ((drd = readdir(dir)) != NULL) {
		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(filename, MAX_PF_NAME, "%s/%s/%s", dfile, drd->d_name, S_STAT);
		filename[MAX_PF_NAME - 1] = '\0';

		/* Read current partition stats */
		read_sysfs_file_stat(curr, filename, drd->d_name);
	}

	/* Close device directory */
	closedir(dir);
}

/*
 ***************************************************************************
 * Read stats from the sysfs filesystem for the devices entered on the
 * command line.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_sysfs_dlist_stat(int curr)
{
	int dev, ok;
	char filename[MAX_PF_NAME];
	char *slash;
	struct io_dlist *st_dev_list_i;

	/* Every I/O device (or partition) is potentially unregistered */
	set_entries_inactive(iodev_nr, st_hdr_iodev);

	for (dev = 0; dev < dlist_idx; dev++) {
		st_dev_list_i = st_dev_list + dev;

		/* Some devices may have a slash in their name (eg. cciss/c0d0...) */
		while ((slash = strchr(st_dev_list_i->dev_name, '/'))) {
			*slash = '!';
		}

		snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
			 SYSFS_BLOCK, st_dev_list_i->dev_name, S_STAT);
		filename[MAX_PF_NAME - 1] = '\0';

		/* Read device stats */
		ok = read_sysfs_file_stat(curr, filename, st_dev_list_i->dev_name);

		if (ok && st_dev_list_i->disp_part) {
			/* Also read stats for its partitions */
			read_sysfs_dlist_part_stat(curr, st_dev_list_i->dev_name);
		}
	}

	/* Free structures corresponding to unregistered devices */
	free_inactive_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read stats from the sysfs filesystem for every block devices found.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_sysfs_stat(int curr)
{
	DIR *dir;
	struct dirent *drd;
	char filename[MAX_PF_NAME];
	int ok;

	/* Every I/O device entry is potentially unregistered */
	set_entries_inactive(iodev_nr, st_hdr_iodev);

	/* Open /sys/block directory */
	if ((dir = opendir(SYSFS_BLOCK)) != NULL) {

		/* Get current entry */
		while ((drd = readdir(dir)) != NULL) {
			if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
				continue;
			snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
				 SYSFS_BLOCK, drd->d_name, S_STAT);
			filename[MAX_PF_NAME - 1] = '\0';
	
			/* If current entry is a directory, try to read its stat file */
			ok = read_sysfs_file_stat(curr, filename, drd->d_name);
	
			/*
			 * If '-p ALL' was entered on the command line,
			 * also try to read stats for its partitions
			 */
			if (ok && DISPLAY_PART_ALL(flags)) {
				read_sysfs_dlist_part_stat(curr, drd->d_name);
			}
		}

		/* Close /sys/block directory */
		closedir(dir);
	}

	/* Free structures corresponding to unregistered devices */
	free_inactive_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read stats from /proc/diskstats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_diskstats_stat(int curr)
{
	FILE *fp;
	char line[256], dev_name[MAX_NAME_LEN];
	char *dm_name;
	struct io_stats sdev;
	int i;
	unsigned long rd_ios, rd_merges_or_rd_sec, rd_ticks_or_wr_sec, wr_ios;
	unsigned long ios_pgr, tot_ticks, rq_ticks, wr_merges, wr_ticks;
	unsigned long long rd_sec_or_wr_ios, wr_sec;
	char *ioc_dname;
	unsigned int major, minor;

	/* Every I/O device entry is potentially unregistered */
	set_entries_inactive(iodev_nr, st_hdr_iodev);

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		return;

	while (fgets(line, 256, fp) != NULL) {

		/* major minor name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq */
		i = sscanf(line, "%u %u %s %lu %lu %llu %lu %lu %lu %llu %lu %lu %lu %lu",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
			   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks);

		if (i == 14) {
			/* Device or partition */
			if (!dlist_idx && !DISPLAY_PARTITIONS(flags) && !is_device(dev_name))
				continue;
			sdev.rd_ios     = rd_ios;
			sdev.rd_merges  = rd_merges_or_rd_sec;
			sdev.rd_sectors = rd_sec_or_wr_ios;
			sdev.rd_ticks   = rd_ticks_or_wr_sec;
			sdev.wr_ios     = wr_ios;
			sdev.wr_merges  = wr_merges;
			sdev.wr_sectors = wr_sec;
			sdev.wr_ticks   = wr_ticks;
			sdev.ios_pgr    = ios_pgr;
			sdev.tot_ticks  = tot_ticks;
			sdev.rq_ticks   = rq_ticks;
		}
		else if (i == 7) {
			/* Partition without extended statistics */
			if (DISPLAY_EXTENDED(flags) ||
			    (!dlist_idx && !DISPLAY_PARTITIONS(flags)))
				continue;

			sdev.rd_ios     = rd_ios;
			sdev.rd_sectors = rd_merges_or_rd_sec;
			sdev.wr_ios     = rd_sec_or_wr_ios;
			sdev.wr_sectors = rd_ticks_or_wr_sec;
		}
		else
			/* Unknown entry: Ignore it */
			continue;

		if ((ioc_dname = ioc_name(major, minor)) != NULL) {
			if (strcmp(dev_name, ioc_dname) && strcmp(ioc_dname, K_NODEV)) {
				/*
				 * No match: Use name generated from sysstat.ioconf data
				 * (if different from "nodev") works around known issues
				 * with EMC PowerPath.
				 */
				strncpy(dev_name, ioc_dname, MAX_NAME_LEN);
			}
		}

		if ((DISPLAY_DEVMAP_NAME(flags)) && (major == DEVMAP_MAJOR)) {
			/*
			 * If the device is a device mapper device, try to get its
			 * assigned name of its logical device.
			 */
			dm_name = transform_devmapname(major, minor);
			if (dm_name) {
				strcpy(dev_name, dm_name);
			}
		}

		save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);
	}
	fclose(fp);

	/* Free structures corresponding to unregistered devices */
	free_inactive_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read stats from /proc/partitions.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_ppartitions_stat(int curr)
{
	FILE *fp;
	char line[256], dev_name[MAX_NAME_LEN];
	struct io_stats sdev;
	unsigned long rd_ios, rd_merges, rd_ticks, wr_ios, wr_merges, wr_ticks;
	unsigned long ios_pgr, tot_ticks, rq_ticks;
	unsigned long long rd_sec, wr_sec;
	char *ioc_dname, *dm_name;
	unsigned int major, minor;

	/* Every I/O device entry is potentially unregistered */
	set_entries_inactive(iodev_nr, st_hdr_iodev);

	if ((fp = fopen(PPARTITIONS, "r")) == NULL)
		return;

	while (fgets(line, 256, fp) != NULL) {
		/* major minor #blocks name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq */
		if (sscanf(line, "%u %u %*u %s %lu %lu %llu %lu %lu %lu %llu"
			   " %lu %lu %lu %lu",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_merges, &rd_sec, &rd_ticks, &wr_ios, &wr_merges,
			   &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks) == 14) {
			/* Device or partition */
			sdev.rd_ios     = rd_ios;  sdev.rd_merges = rd_merges;
			sdev.rd_sectors = rd_sec;  sdev.rd_ticks  = rd_ticks;
			sdev.wr_ios     = wr_ios;  sdev.wr_merges = wr_merges;
			sdev.wr_sectors = wr_sec;  sdev.wr_ticks  = wr_ticks;
			sdev.ios_pgr    = ios_pgr; sdev.tot_ticks = tot_ticks;
			sdev.rq_ticks   = rq_ticks;
		}
		else
			/* Unknown entry: Ignore it */
			continue;

		if ((ioc_dname = ioc_name(major, minor)) != NULL) {
			if (strcmp(dev_name, ioc_dname) && strcmp(ioc_dname, K_NODEV)) {
				/* Compensate for EMC PowerPath driver bug */
				strncpy(dev_name, ioc_dname, MAX_NAME_LEN);
			}
		}

		if ((DISPLAY_DEVMAP_NAME(flags)) && (major == DEVMAP_MAJOR)) {
			/* Get device mapper logical name */
			dm_name = transform_devmapname(major, minor);
			if (dm_name) {
				strcpy(dev_name, dm_name);
			}
		}

		save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);
	}
	fclose(fp);

	/* Free structures corresponding to unregistered devices */
	free_inactive_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read NFS-mount directories stats from /proc/self/mountstats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_nfs_stat(int curr)
{
	FILE *fp;
	int sw = 0;
	char line[8192];
	char *xprt_line;
	char nfs_name[MAX_NAME_LEN];
	char mount[10], on[10], prefix[10], aux[32];
	char operation[16];
	struct io_nfs_stats snfs;
	long int v1;

	/* Every I/O NFS entry is potentially unregistered */
	set_entries_inactive(ionfs_nr, st_hdr_ionfs);

	if ((fp = fopen(NFSMOUNTSTATS, "r")) == NULL)
		return;

	sprintf(aux, "%%%ds %%10s %%10s",
		MAX_NAME_LEN < 200 ? MAX_NAME_LEN : 200);

	while (fgets(line, 256, fp) != NULL) {

		/* read NFS directory name */
		if (!strncmp(line, "device", 6)) {
			sw = 0;
			sscanf(line + 6, aux, nfs_name, mount, on);
			if ((!strncmp(mount, "mounted", 7)) && (!strncmp(on, "on", 2))) {
				sw = 1;
			}
		}

		sscanf(line, "%10s", prefix);
		if (sw && (!strncmp(prefix, "bytes:", 6))) {
			/* Read the stats for the last NFS-mounted directory */
			sscanf(strstr(line, "bytes:") + 6, "%llu %llu %llu %llu %llu %llu",
			       &snfs.rd_normal_bytes, &snfs.wr_normal_bytes,
			       &snfs.rd_direct_bytes, &snfs.wr_direct_bytes,
			       &snfs.rd_server_bytes, &snfs.wr_server_bytes);
			sw = 2;
		}

		if ((sw == 2) && (!strncmp(prefix, "xprt:", 5))) {
			/*
			 * Read extended statistic for the last NFS-mounted directory
			 * - number of sent rpc requests.
			 */
			xprt_line = (strstr(line, "xprt:") + 6);
			/* udp, tcp or rdma data */
			if (!strncmp(xprt_line, "udp", 3)) {
				/* port bind_count sends recvs (bad_xids req_u bklog_u) */
				sscanf(strstr(xprt_line, "udp") + 4, "%*u %*u %lu",
				       &snfs.rpc_sends);
			}
			if (!strncmp(xprt_line, "tcp", 3)) {
				/*
				 * port bind_counter connect_count connect_time idle_time
				 * sends recvs (bad_xids req_u bklog_u)
				 */
				sscanf(strstr(xprt_line, "tcp") + 4,
				       "%*u %*u %*u %*u %*d %lu",
				       &snfs.rpc_sends);
			}
			if (!strncmp(xprt_line,"rdma", 4)) {
				/*
				 * 0(port) bind_count connect_count connect_time idle_time
				 * sends recvs (bad_xids req_u bklog_u...)
				 */
				sscanf(strstr(xprt_line, "rdma") + 5,
				       "%*u %*u %*u %*u %*d %lu",
				       &snfs.rpc_sends);
			}
			sw = 3;
		}

		if ((sw == 3) && (!strncmp(prefix, "per-op", 6))) {
			sw = 4;
			while (sw == 4) {
				fgets(line, 256, fp);
				sscanf(line, "%15s %lu", operation, &v1);
				if (!strncmp(operation, "READ:", 5)) {
					snfs.nfs_rops = v1;
				}
				else if (!strncmp(operation, "WRITE:", 6)) {
					snfs.nfs_wops = v1;
					save_stats(nfs_name, curr, &snfs, ionfs_nr, st_hdr_ionfs);
					sw = 0;
				}
			}
		}
	}

	fclose(fp);

	/* Free structures corresponding to unregistered devices */
	free_inactive_entries(ionfs_nr, st_hdr_ionfs);
}

/*
 ***************************************************************************
 * Display CPU utilization.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time.
 ***************************************************************************
 */
void write_cpu_stat(int curr, unsigned long long itv)
{
	printf("avg-cpu:  %%user   %%nice %%system %%iowait  %%steal   %%idle\n");

	printf("         %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f\n\n",
	       ll_sp_value(st_cpu[!curr]->cpu_user,   st_cpu[curr]->cpu_user,   itv),
	       ll_sp_value(st_cpu[!curr]->cpu_nice,   st_cpu[curr]->cpu_nice,   itv),
		/*
		 * Time spent in system mode also includes time spent servicing
		 * hard and soft interrupts.
		 */
	       ll_sp_value(st_cpu[!curr]->cpu_sys + st_cpu[!curr]->cpu_softirq +
			   st_cpu[!curr]->cpu_hardirq,
			   st_cpu[curr]->cpu_sys + st_cpu[curr]->cpu_softirq +
			   st_cpu[curr]->cpu_hardirq, itv),
	       ll_sp_value(st_cpu[!curr]->cpu_iowait, st_cpu[curr]->cpu_iowait, itv),
	       ll_sp_value(st_cpu[!curr]->cpu_steal,  st_cpu[curr]->cpu_steal,  itv),
	       (st_cpu[curr]->cpu_idle < st_cpu[!curr]->cpu_idle) ?
	       0.0 :
	       ll_sp_value(st_cpu[!curr]->cpu_idle,   st_cpu[curr]->cpu_idle,   itv));
}

/*
 ***************************************************************************
 * Display disk stats header.
 *
 * OUT:
 * @fctr	Conversion factor.
 ***************************************************************************
 */
void write_disk_stat_header(int *fctr)
{
	if (DISPLAY_EXTENDED(flags)) {
		/* Extended stats */
		printf("Device:         rrqm/s   wrqm/s     r/s     w/s");
		if (DISPLAY_MEGABYTES(flags)) {
			printf("    rMB/s    wMB/s");
			*fctr = 2048;
		}
		else if (DISPLAY_KILOBYTES(flags)) {
			printf("    rkB/s    wkB/s");
			*fctr = 2;
		}
		else {
			printf("   rsec/s   wsec/s");
		}
		printf(" avgrq-sz avgqu-sz   await  svctm  %%util\n");
	}
	else {
		/* Basic stats */
		printf("Device:            tps");
		if (DISPLAY_KILOBYTES(flags)) {
			printf("    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn\n");
			*fctr = 2;
		}
		else if (DISPLAY_MEGABYTES(flags)) {
			printf("    MB_read/s    MB_wrtn/s    MB_read    MB_wrtn\n");
			*fctr = 2048;
		}
		else {
			printf("   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n");
		}
	}
}

/*
 ***************************************************************************
 * Display NFS stats header.
 *
 * OUT:
 * @fctr	Conversion factor.
 ***************************************************************************
 */
void write_nfs_stat_header(int *fctr)
{
	printf("Filesystem:           ");
	if (DISPLAY_KILOBYTES(flags)) {
		printf("    rkB_nor/s    wkB_nor/s    rkB_dir/s    wkB_dir/s"
		       "    rkB_svr/s    wkB_svr/s");
		*fctr = 1024;
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		printf("    rMB_nor/s    wMB_nor/s    rMB_dir/s    wMB_dir/s"
		       "    rMB_svr/s    wMB_svr/s");
		*fctr = 1024 * 1024;
	}
	else {
		printf("   rBlk_nor/s   wBlk_nor/s   rBlk_dir/s   wBlk_dir/s"
		       "   rBlk_svr/s   wBlk_svr/s");
		*fctr = 512;
	}
	printf("     ops/s    rops/s    wops/s\n");
}

/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 ***************************************************************************
 */
void write_ext_stat(int curr, unsigned long long itv, int fctr,
		    struct io_hdr_stats *shi, struct io_stats *ioi,
		    struct io_stats *ioj)
{
	struct stats_disk sdc, sdp;
	struct ext_disk_stats xds;
	
	/*
	 * Counters overflows are possible, but don't need to be handled in
	 * a special way: the difference is still properly calculated if the
	 * result is of the same type as the two values.
	 * Exception is field rq_ticks which is incremented by the number of
	 * I/O in progress times the number of milliseconds spent doing I/O.
	 * But the number of I/O in progress (field ios_pgr) happens to be
	 * sometimes negative...
	 */
	sdc.nr_ios    = ioi->rd_ios + ioi->wr_ios;
	sdp.nr_ios    = ioj->rd_ios + ioj->wr_ios;

	sdc.tot_ticks = ioi->tot_ticks;
	sdp.tot_ticks = ioj->tot_ticks;

	sdc.rd_ticks  = ioi->rd_ticks;
	sdp.rd_ticks  = ioj->rd_ticks;
	sdc.wr_ticks  = ioi->wr_ticks;
	sdp.wr_ticks  = ioj->wr_ticks;

	sdc.rd_sect   = ioi->rd_sectors;
	sdp.rd_sect   = ioj->rd_sectors;
	sdc.wr_sect   = ioi->wr_sectors;
	sdp.wr_sect   = ioj->wr_sectors;
	
	compute_ext_disk_stats(&sdc, &sdp, itv, &xds);
	
	/*      DEV   rrq/s wrq/s   r/s   w/s  rsec  wsec  rqsz  qusz await svctm %util */
	printf("%-13s %8.2f %8.2f %7.2f %7.2f %8.2f %8.2f %8.2f %8.2f %7.2f %6.2f %6.2f\n",
	       shi->name,
	       S_VALUE(ioj->rd_merges, ioi->rd_merges, itv),
	       S_VALUE(ioj->wr_merges, ioi->wr_merges, itv),
	       S_VALUE(ioj->rd_ios, ioi->rd_ios, itv),
	       S_VALUE(ioj->wr_ios, ioi->wr_ios, itv),
	       ll_s_value(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
	       ll_s_value(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
	       xds.arqsz,
	       S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0,
	       xds.await,
	       /* The ticks output is biased to output 1000 ticks per second */
	       xds.svctm,
	       /* Again: Ticks in milliseconds */
	       xds.util / 10.0);
}

/*
 ***************************************************************************
 * Write basic stats, read from /proc/stat, /proc/{diskstats,partitions}
 * or from sysfs.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 ***************************************************************************
 */
void write_basic_stat(int curr, unsigned long long itv, int fctr,
		      struct io_hdr_stats *shi, struct io_stats *ioi,
		      struct io_stats *ioj)
{
	unsigned long long rd_sec, wr_sec;

	printf("%-13s", shi->name);

	if (HAS_SYSFS(flags) ||
	    HAS_DISKSTATS(flags) || HAS_PPARTITIONS(flags)) {
		/* Print stats coming from /sys or /proc/{diskstats,partitions} */
		rd_sec = ioi->rd_sectors - ioj->rd_sectors;
		if ((ioi->rd_sectors < ioj->rd_sectors) && (ioj->rd_sectors <= 0xffffffff)) {
			rd_sec &= 0xffffffff;
		}
		wr_sec = ioi->wr_sectors - ioj->wr_sectors;
		if ((ioi->wr_sectors < ioj->wr_sectors) && (ioj->wr_sectors <= 0xffffffff)) {
			wr_sec &= 0xffffffff;
		}

		printf(" %8.2f %12.2f %12.2f %10llu %10llu\n",
		       S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv),
		       ll_s_value(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
		       ll_s_value(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
		       (unsigned long long) rd_sec / fctr,
		       (unsigned long long) wr_sec / fctr);
	}
	else {
		/* Print stats coming from /proc/stat */
		printf(" %8.2f %12.2f %12.2f %10lu %10lu\n",
		       S_VALUE(ioj->dk_drive, ioi->dk_drive, itv),
		       S_VALUE(ioj->dk_drive_rblk, ioi->dk_drive_rblk, itv) / fctr,
		       S_VALUE(ioj->dk_drive_wblk, ioi->dk_drive_wblk, itv) / fctr,
		       (ioi->dk_drive_rblk - ioj->dk_drive_rblk) / fctr,
		       (ioi->dk_drive_wblk - ioj->dk_drive_wblk) / fctr);
	}
}


/*
 ***************************************************************************
 * Write NFS stats read from /proc/self/mountstats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the NFS filesystems.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 ***************************************************************************
 */
void write_nfs_stat(int curr, unsigned long long itv, int fctr,
		    struct io_hdr_stats *shi, struct io_nfs_stats *ioni,
		    struct io_nfs_stats *ionj)
{
	if (DISPLAY_HUMAN_READ(flags)) {
		printf("%-22s\n%23s", shi->name, "");
	}
	else {
		printf("%-22s ", shi->name);
	}
	printf("%12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %9.2f %9.2f %9.2f\n",
	       S_VALUE(ionj->rd_normal_bytes, ioni->rd_normal_bytes, itv) / fctr,
	       S_VALUE(ionj->wr_normal_bytes, ioni->wr_normal_bytes, itv) / fctr,
	       S_VALUE(ionj->rd_direct_bytes, ioni->rd_direct_bytes, itv) / fctr,
	       S_VALUE(ionj->wr_direct_bytes, ioni->wr_direct_bytes, itv) / fctr,
	       S_VALUE(ionj->rd_server_bytes, ioni->rd_server_bytes, itv) / fctr,
	       S_VALUE(ionj->wr_server_bytes, ioni->wr_server_bytes, itv) / fctr,
	       S_VALUE(ionj->rpc_sends, ioni->rpc_sends, itv),
	       S_VALUE(ionj->nfs_rops,  ioni->nfs_rops,  itv),
	       S_VALUE(ionj->nfs_wops,  ioni->nfs_wops,  itv));
}

/*
 ***************************************************************************
 * Print everything now (stats and uptime).
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void write_stats(int curr, struct tm *rectime)
{
	int dev, i, fctr = 1;
	unsigned long long itv;
	struct io_hdr_stats *shi;
	struct io_dlist *st_dev_list_i;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	/* Print time stamp */
	if (DISPLAY_TIMESTAMP(flags)) {
		if (DISPLAY_ISO(flags)) {
			strftime(timestamp, sizeof(timestamp), "%FT%T%z", rectime);
		}
		else {
			strftime(timestamp, sizeof(timestamp), "%x %X", rectime);
		}
		printf("%s\n", timestamp);
	}

	/* Interval is multiplied by the number of processors */
	itv = get_interval(uptime[!curr], uptime[curr]);

	if (DISPLAY_CPU(flags)) {
		/* Display CPU utilization */
		write_cpu_stat(curr, itv);
	}

	if (cpu_nr > 1) {
		/* On SMP machines, reduce itv to one processor (see note above) */
		itv = get_interval(uptime0[!curr], uptime0[curr]);
	}

	if (DISPLAY_DISK(flags)) {
		struct io_stats *ioi, *ioj;

		shi = st_hdr_iodev;

		/* Display disk stats header */
		write_disk_stat_header(&fctr);

		if (DISPLAY_EXTENDED(flags) &&
		    (HAS_OLD_KERNEL(flags) || HAS_PLAIN_KERNEL24(flags))) {
			/* No extended stats with old 2.2-2.4 kernels */
			printf("\n");
			return;
		}

		for (i = 0; i < iodev_nr; i++, shi++) {
			if (shi->used) {
	
				if (dlist_idx && !HAS_SYSFS(flags)) {
					/*
					 * With sysfs, only stats for the requested
					 * devices are read.
					 * With /proc/{diskstats,partitions}, stats for
					 * every device are read. Thus we need to check
					 * if stats for current device are to be displayed.
					 */
					for (dev = 0; dev < dlist_idx; dev++) {
						st_dev_list_i = st_dev_list + dev;
						if (!strcmp(shi->name, st_dev_list_i->dev_name))
							break;
					}
					if (dev == dlist_idx)
						/* Device not found in list: Don't display it */
						continue;
				}
	
				ioi = st_iodev[curr] + i;
				ioj = st_iodev[!curr] + i;

				if (!DISPLAY_UNFILTERED(flags)) {
					if (HAS_OLD_KERNEL(flags) ||
					    HAS_PLAIN_KERNEL24(flags)) {
						if (!ioi->dk_drive)
							continue;
					}
					else {
						if (!ioi->rd_ios && !ioi->wr_ios)
							continue;
					}
				}
				
				if (DISPLAY_ZERO_OMIT(flags)) {
					if (HAS_OLD_KERNEL(flags) ||
					    HAS_PLAIN_KERNEL24(flags)) {
						if (ioi->dk_drive == ioj->dk_drive)
							/* No activity: Ignore it */
							continue;
					}
					else {
						if ((ioi->rd_ios == ioj->rd_ios) &&
						    (ioi->wr_ios == ioj->wr_ios))
							/* No activity: Ignore it */
							continue;
					}
				}

				if (DISPLAY_EXTENDED(flags)) {
					write_ext_stat(curr, itv, fctr, shi, ioi, ioj);
				}
				else {
					write_basic_stat(curr, itv, fctr, shi, ioi, ioj);
				}
			}
		}
		printf("\n");
	}

	if (DISPLAY_NFS(flags)) {
		struct io_nfs_stats *ioni, *ionj;

		shi = st_hdr_ionfs;

		/* Display NFS stats header */
		write_nfs_stat_header(&fctr);

		if (!HAS_NFS(flags)) {
			/* No NFS stats */
			printf("\n");
			return;
		}

		for (i = 0; i < ionfs_nr; i++, shi++) {
			if (shi->used) {
	
				ioni = st_ionfs[curr] + i;
				ionj = st_ionfs[!curr] + i;
				write_nfs_stat(curr, itv, fctr, shi, ioni, ionj);
			}
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Main loop: Read I/O stats from the relevant sources and display them.
 *
 * IN:
 * @count	Number of lines of stats to print.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void rw_io_stat_loop(long int count, struct tm *rectime)
{
	int curr = 1;

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);
	
	do {
		if (cpu_nr > 1) {
			/*
			 * Read system uptime (only for SMP machines).
			 * Init uptime0. So if /proc/uptime cannot fill it,
			 * this will be done by /proc/stat.
			 */
			uptime0[curr] = 0;
			read_uptime(&(uptime0[curr]));
		}

		/*
		 * Read stats for CPU "all" and 0.
		 * Note that stats for CPU 0 are not used per se. It only makes
		 * read_stat_cpu() fill uptime0.
		 */
		read_stat_cpu(st_cpu[curr], 2, &(uptime[curr]), &(uptime0[curr]));

		/*
		 * If we don't want extended statistics, and if /proc/diskstats and
		 * /proc/partitions don't exist, and /sys is not mounted, then
		 * we try to get disks stats from /proc/stat.
		 */
		if (!DISPLAY_EXTENDED(flags) && !HAS_DISKSTATS(flags) &&
		    !HAS_PPARTITIONS(flags) && !HAS_SYSFS(flags)) {
			read_proc_stat(curr);
		}

		if (dlist_idx) {
			/*
			 * A device or partition name was entered on the command line,
			 * with or without -p option (but not -p ALL).
			 */
			if (HAS_DISKSTATS(flags) && !DISPLAY_PARTITIONS(flags)) {
				read_diskstats_stat(curr);
			}
			else if (HAS_SYSFS(flags)) {
				read_sysfs_dlist_stat(curr);
			}
			else if (HAS_PPARTITIONS(flags) && !DISPLAY_PARTITIONS(flags)) {
				read_ppartitions_stat(curr);
			}
		}
		else {
			/*
			 * No devices nor partitions entered on the command line
			 * (for example if -p ALL was used).
			 */
			if (HAS_DISKSTATS(flags)) {
				read_diskstats_stat(curr);
			}
			else if (HAS_SYSFS(flags)) {
				read_sysfs_stat(curr);
			}
			else if (HAS_PPARTITIONS(flags)) {
				read_ppartitions_stat(curr);
			}
		}

		/* Read NFS directories stats */
		if (HAS_NFS(flags)) {
			read_nfs_stat(curr);
		}

		/* Get time */
		get_localtime(rectime);

		/* Print results */
		write_stats(curr, rectime);

		if (count > 0) {
			count--;
		}
		if (count) {
			curr ^= 1;
			pause();
		}
	}
	while (count);
}

/*
 ***************************************************************************
 * Main entry to the iostat program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int it = 0;
	int opt = 1;
	int i;
	long count = 1;
	struct utsname header;
	struct io_dlist *st_dev_list_i;
	struct tm rectime;
	char *t;

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	/* Get HZ */
	get_HZ();

	/* Allocate structures for device list */
	if (argc > 1) {
		salloc_dev_list(argc - 1 + count_csvalues(argc, argv));
	}

	/* Process args... */
	while (opt < argc) {

		if (!strcmp(argv[opt], "-p")) {
			flags |= I_D_PARTITIONS;
			if (argv[++opt] &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt])) &&
			    (strncmp(argv[opt], "-", 1))) {
				flags |= I_D_UNFILTERED;
				
				for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
					if (!strcmp(t, K_ALL)) {
						flags |= I_D_PART_ALL;
					}
					else {
						/* Store device name */
						i = update_dev_list(&dlist_idx, device_name(t));
						st_dev_list_i = st_dev_list + i;
						st_dev_list_i->disp_part = TRUE;
					}
				}
				opt++;
			}
			else {
				flags |= I_D_PART_ALL;
			}
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'c':
					/* Display cpu usage */
					flags |= I_D_CPU;
					break;

				case 'd':
					/* Display disk utilization */
					flags |= I_D_DISK;
					break;

				case 'h':
					/* Display an easy-to-read NFS report */
					flags |= I_D_HUMAN_READ;
					break;
	
				case 'k':
					if (DISPLAY_MEGABYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in kB/s */
					flags |= I_D_KILOBYTES;
					break;

				case 'm':
					if (DISPLAY_KILOBYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in MB/s */
					flags |= I_D_MEGABYTES;
					break;
	
				case 'N':
					/* Display device mapper logical name */
					flags |= I_D_DEVMAP_NAME;
					break;
	
				case 'n':
					/* Display NFS stats */
					flags |= I_D_NFS;
					break;

				case 't':
					/* Display timestamp */
					flags |= I_D_TIMESTAMP;
					break;
	
				case 'x':
					/* Display extended stats */
					flags |= I_D_EXTENDED;
					break;
					
				case 'z':
					/* Omit output for devices with no activity */
					flags |= I_D_ZERO_OMIT;
					break;

				case 'V':
					/* Print version number and exit */
					print_version();
					break;
	
				default:
					usage(argv[0]);
				}
			}
			opt++;
		}

		else if (!isdigit(argv[opt][0])) {
			flags |= I_D_UNFILTERED;
			if (strcmp(argv[opt], K_ALL)) {
				/* Store device name */
				update_dev_list(&dlist_idx, device_name(argv[opt++]));
			}
			else {
				opt++;
			}
		}

		else if (!it) {
			interval = atol(argv[opt++]);
			if (interval < 0) {
				usage(argv[0]);
			}
			count = -1;
			it = 1;
		}

		else if (it > 0) {
			count = atol(argv[opt++]);
			if ((count < 1) || !interval) {
				usage(argv[0]);
			}
			it = -1;
		}
		else {
			usage(argv[0]);
		}
	}

	if (!interval) {
		count = 1;
	}
	
	/* Default: Display CPU and DISK reports */
	if (!DISPLAY_CPU(flags) && !DISPLAY_DISK(flags) && !DISPLAY_NFS(flags)) {
		flags |= I_D_CPU + I_D_DISK;
	}
	/*
	 * Also display DISK reports if options -p, -x or a device has been entered
	 * on the command line.
	 */
	if (DISPLAY_PARTITIONS(flags) || DISPLAY_EXTENDED(flags) ||
	    DISPLAY_UNFILTERED(flags)) {
		flags |= I_D_DISK;
	}

	/* Ignore device list if '-p ALL' entered on the command line */
	if (DISPLAY_PART_ALL(flags)) {
		dlist_idx = 0;
	}

	/* Init structures according to machine architecture */
	io_sys_init();

	get_localtime(&rectime);

	/* Get system name, release number and hostname */
	uname(&header);
	if (print_gal_header(&rectime, header.sysname, header.release,
			     header.nodename, header.machine, cpu_nr)) {
		flags |= I_D_ISO;
	}
	printf("\n");

	/* Set a handler for SIGALRM */
	alarm_handler(0);

	/* Main loop */
	rw_io_stat_loop(count, &rectime);

	/* Free structures */
	io_sys_free();
	sfree_dev_list();
	
	return 0;
}
