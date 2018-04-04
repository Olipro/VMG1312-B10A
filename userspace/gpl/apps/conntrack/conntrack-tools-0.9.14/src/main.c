/*
 * (C) 2006-2007 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "conntrackd.h"
#include "log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <limits.h>

struct ct_general_state st;
union ct_state state;

static const char usage_daemon_commands[] =
	"Daemon mode commands:\n"
	"  -d [options]\t\tRun in daemon mode\n";

static const char usage_client_commands[] =
	"Client mode commands:\n"
	"  -c, commit external cache to conntrack table\n"
	"  -f [|internal|external], flush internal and external cache\n"
	"  -F, flush kernel conntrack table\n"
	"  -i, display content of the internal cache\n"
	"  -e, display the content of the external cache\n"
	"  -k, kill conntrack daemon\n"
	"  -s  [|network|cache|runtime|link|rsqueue|queue], dump statistics\n"
	"  -R, resync with kernel conntrack table\n"
	"  -n, request resync with other node (only FT-FW and NOTRACK modes)\n"
	"  -x, dump cache in XML format (requires -i or -e)\n"
	"  -t, reset the kernel timeout (see PurgeTimeout clause)\n"
	"  -v, display conntrackd version\n"
	"  -h, display this help information\n";

static const char usage_options[] =
	"Options:\n"
	"  -C [configfile], configuration file path\n";

static void
show_usage(char *progname)
{
	fprintf(stdout, "Connection tracking userspace daemon v%s\n", VERSION);
	fprintf(stdout, "Usage: %s [commands] [options]\n\n", progname);
	fprintf(stdout, "%s\n", usage_daemon_commands);
	fprintf(stdout, "%s\n", usage_client_commands);
	fprintf(stdout, "%s\n", usage_options);
}

static void
show_version(void)
{
	fprintf(stdout, "Connection tracking userspace daemon v%s. ", VERSION);
	fprintf(stdout, "Licensed under GPLv2.\n");
	fprintf(stdout, "(C) 2006-2009 Pablo Neira Ayuso ");
	fprintf(stdout, "<pablo@netfilter.org>\n");
}

static void
set_operation_mode(int *current, int want, char *argv[])
{
	if (*current == NOT_SET) {
		*current = want;
		return;
	}
	if (*current != want) {
		show_usage(argv[0]);
		fprintf(stderr, "\nError: Invalid parameters\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	int ret, i, action = -1;
	char config_file[PATH_MAX] = {};
	int type = 0;
	struct utsname u;
	int version, major, minor;

	/* Check kernel version: it must be >= 2.6.18 */
	if (uname(&u) == -1) {
		fprintf(stderr, "Can't retrieve kernel version via uname()\n");
		exit(EXIT_FAILURE);
	}
	sscanf(u.release, "%d.%d.%d", &version, &major, &minor);
	if (version < 2 && major < 6 && minor < 18) {
		fprintf(stderr, "Linux kernel version must be >= 2.6.18\n");
		exit(EXIT_FAILURE);
	}

	for (i=1; i<argc; i++) {
		switch(argv[i][1]) {
		case 'd':
			set_operation_mode(&type, DAEMON, argv);
			break;
		case 'c':
			set_operation_mode(&type, REQUEST, argv);
			action = COMMIT;
			break;
		case 'i':
			set_operation_mode(&type, REQUEST, argv);
			action = DUMP_INTERNAL;
			break;
		case 'e':
			set_operation_mode(&type, REQUEST, argv);
			action = DUMP_EXTERNAL;
			break;
		case 'C':
			if (++i < argc) {
				strncpy(config_file, argv[i], PATH_MAX);
				if (strlen(argv[i]) >= PATH_MAX){
					config_file[PATH_MAX-1]='\0';
					fprintf(stderr, "Path to config file "
						        "to long. Cutting it "
							"down to %d characters",
							PATH_MAX);
				}
				break;
			}
			show_usage(argv[0]);
			fprintf(stderr, "Missing config filename\n");
			break;
		case 'F':
			set_operation_mode(&type, REQUEST, argv);
			action = FLUSH_MASTER;
			break;
		case 'f':
			set_operation_mode(&type, REQUEST, argv);
			if (i+1 < argc && argv[i+1][0] != '-') {
				if (strncmp(argv[i+1], "internal",
					    strlen(argv[i+1])) == 0) {
					action = FLUSH_INT_CACHE;
					i++;
				} else if (strncmp(argv[i+1], "external",
						 strlen(argv[i+1])) == 0) {
					action = FLUSH_EXT_CACHE;
					i++;
				} else {
					fprintf(stderr, "ERROR: unknown "
							"parameter `%s' for "
							"option `-f'\n",
							argv[i+1]);
					exit(EXIT_FAILURE);
				}
			} else {
				/* default to general flushing */
				action = FLUSH_CACHE;
			}
			break;
		case 'R':
			set_operation_mode(&type, REQUEST, argv);
			action = RESYNC_MASTER;
			break;
		case 'B':
			set_operation_mode(&type, REQUEST, argv);
			action = SEND_BULK;
			break;
		case 't':
			set_operation_mode(&type, REQUEST, argv);
			action = RESET_TIMERS;
			break;
		case 'k':
			set_operation_mode(&type, REQUEST, argv);
			action = KILL;
			break;
		case 's':
			set_operation_mode(&type, REQUEST, argv);
			/* we've got a parameter */
			if (i+1 < argc && argv[i+1][0] != '-') {
				if (strncmp(argv[i+1], "network",
					    strlen(argv[i+1])) == 0) {
					action = STATS_NETWORK;
					i++;
				} else if (strncmp(argv[i+1], "cache",
						 strlen(argv[i+1])) == 0) {
					action = STATS_CACHE;
					i++;
				} else if (strncmp(argv[i+1], "runtime",
						 strlen(argv[i+1])) == 0) {
					action = STATS_RUNTIME;
					i++;
				} else if (strncmp(argv[i+1], "multicast",
						 strlen(argv[i+1])) == 0) {
					fprintf(stderr, "WARNING: use `link' "
						"instead of `multicast' as "
						"parameter.\n");
					action = STATS_LINK;
					i++;
				} else if (strncmp(argv[i+1], "link",
						 strlen(argv[i+1])) == 0) {
					action = STATS_LINK;
					i++;
				} else if (strncmp(argv[i+1], "rsqueue",
						strlen(argv[i+1])) == 0) {
					action = STATS_RSQUEUE;
					i++;
				} else if (strncmp(argv[i+1], "process",
						 strlen(argv[i+1])) == 0) {
					action = STATS_PROCESS;
					i++;
				} else if (strncmp(argv[i+1], "queue",
						strlen(argv[i+1])) == 0) {
					action = STATS_QUEUE;
					i++;
				} else {
					fprintf(stderr, "ERROR: unknown "
							"parameter `%s' for "
							"option `-s'\n",
							argv[i+1]);
					exit(EXIT_FAILURE);
				}
			} else {
				/* default to general statistics */
				action = STATS;
			}
			break;
		case 'S':
			fprintf(stderr, "WARNING: -S option is obsolete. "
					"Ignoring.\n");
			break;
		case 'n':
			set_operation_mode(&type, REQUEST, argv);
			action = REQUEST_DUMP;
			break;
		case 'x':
			if (action == DUMP_INTERNAL)
				action = DUMP_INT_XML;
			else if (action == DUMP_EXTERNAL)
				action = DUMP_EXT_XML;
			else {
				show_usage(argv[0]);
				fprintf(stderr, "Error: Invalid parameters\n");
				exit(EXIT_FAILURE);

			}
			break;
		case 'v':
			show_version();
			exit(EXIT_SUCCESS);
		case 'h':
			show_usage(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			show_usage(argv[0]);
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return 0;
			break;
		}
	}

	if (!config_file[0])
		strcpy(config_file, DEFAULT_CONFIGFILE);

	umask(0177);

	if ((ret = init_config(config_file)) == -1) {
		fprintf(stderr, "can't open config file `%s'\n", config_file);
		exit(EXIT_FAILURE);
	}

	if (type == REQUEST) {
		if (do_local_request(action, &conf.local, local_step) == -1) {
			fprintf(stderr, "can't connect: is conntrackd "
					"running? appropriate permissions?\n");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}

	/*
	 * Setting up logging
	 */
	if (init_log() == -1)
		exit(EXIT_FAILURE);

	/*
	 * lock file
	 */
	ret = open(CONFIG(lockfile), O_CREAT | O_EXCL | O_TRUNC, 0600);
	if (ret == -1) {
		fprintf(stderr, "lockfile `%s' exists, perhaps conntrackd "
			        "already running?\n", CONFIG(lockfile));
		exit(EXIT_FAILURE);
	}
	close(ret);

	/*
	 * Setting process priority and scheduler
	 */
	nice(CONFIG(nice));

	if (CONFIG(sched).type != SCHED_OTHER) {
		struct sched_param schedparam = {
			.sched_priority = CONFIG(sched).prio,
		};

		ret = sched_setscheduler(0, CONFIG(sched).type, &schedparam);
		if (ret == -1) {
			perror("sched");
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * initialization process
	 */

	if (init() == -1) {
		close_log();
		fprintf(stderr, "ERROR: conntrackd cannot start, please "
				"check the logfile for more info\n");
		unlink(CONFIG(lockfile));
		exit(EXIT_FAILURE);
	}

	chdir("/");
	close(STDIN_FILENO);

	/* Daemonize conntrackd */
	if (type == DAEMON) {
		pid_t pid;

		if ((pid = fork()) == -1) {
			perror("fork has failed: ");
			exit(EXIT_FAILURE);
		} else if (pid)
			exit(EXIT_SUCCESS);

		setsid();

		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		dlog(LOG_NOTICE, "-- starting in daemon mode --");
	} else
		dlog(LOG_NOTICE, "-- starting in console mode --");

	/*
	 * run main process
	 */
	run();
	return 0;
}
