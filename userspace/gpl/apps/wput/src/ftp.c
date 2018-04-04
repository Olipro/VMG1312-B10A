/* Declarations for wput.
   Copyright (C) 2003
   This file is part of wput.

   The wput is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License 
   as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The wput is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   You should have received a copy of the GNU General Public
   License along with the wput; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* This file contains procedures for interacting with the FTP-Server */

#include "ftp.h"
#include "utils.h"
#include "progress.h"
#include "windows.h"
#include "constants.h"
#include "_queue.h"

void makeskip(_fsession * fsession, char * tmp);

/* if direct cwding fails for some reason, try the long way.
 * directiories that do not exist yet are being created if possible */
/* error-levels: ERR_FAILED, ERR_RECONNECT */
int long_do_cwd(_fsession * fsession){
	int res = 0;
	
	char * unescaped = cpy(fsession->target_dname);
	char * tmpbuf = unescaped;
	char * ptr;
	
	clear_path(unescaped);
	if(fsession->ftp->current_directory) {
		unescaped = get_relative_path(fsession->ftp->current_directory, unescaped);
		free(tmpbuf);
		tmpbuf = unescaped;
	}
	
	/* ftp-urls like ftp://server/<dir1>/<dir2>/<dirN>/ will be executed as
		CWD <dir1>
		CWD <dir2>
		CWD <dirN>
	 */
	tmpbuf = strtok(tmpbuf, "/");
	while(tmpbuf) {
		/* take each token, unescape it and cwd there */
		ptr = unescape(cpy(tmpbuf));
		res = try_do_cwd(fsession->ftp, ptr, res);
		free(ptr);
		if(res == ERR_FAILED) {
			ptr = cpy(fsession->target_dname);
			tmpbuf = strtok(ptr, tmpbuf);
			if(tmpbuf) {
				makeskip(fsession, tmpbuf);
				if(fsession->ftp->current_directory)
					free(fsession->ftp->current_directory);
				tmpbuf = strrchr(ptr, '/');
				if(!tmpbuf) tmpbuf = ptr;
				*tmpbuf = 0;
				fsession->ftp->current_directory = cpy(ptr);
			}
			free(ptr);
		}
		/* hack to skip everything-directory */
		if(res == ERR_SKIP) {
			makeskip(fsession, "");
			res = ERR_FAILED;
		}
		if(res == ERR_RECONNECT || res == ERR_FAILED) {
			free(unescaped);
			return res;
		}
		tmpbuf = strtok(NULL, "/");
	}
	free(unescaped);
	return 0;
}
/* first try to change to the directory. if it fails, try to change to the
 * root-directory if path begins with '/'. is this successful, try to MKDIR
 * path and change there again */
/* error-levels: ERR_RECONNECT, ERR_FAILED, ERR_SKIP (failed for '/'), 1 */
int try_do_cwd(ftp_con * ftp, char * path, int mkd) {
	int res;
	if(!strcmp(path, "."))
		return mkd;
	if(!strcmp(path, ".."))
		mkd = 0;
	
	/* don't even try to CWD if we know that we had to create the 
	 * prior directories... */
	if(!mkd) {
		res = ftp_do_cwd(ftp, path);
		if(SOCK_ERROR(res))
			return ERR_RECONNECT;
	}
	if(res < 0 && opt.no_directories)
		return ERR_FAILED;
	
	if(res < 0 || mkd) {
		/* go to the root directory if we are to start there */
		if(path[0] == '/') {
			res = ftp_do_cwd(ftp, "/");
			if(SOCK_ERROR(res))
				return ERR_RECONNECT;
		
			if(res < 0)
				return ERR_SKIP;
			path++;
		}
		
		/* this is vNORMAL because the user should get a notice
		 * when a remote directory is created... */
		res = ftp_do_mkd(ftp, path);
		if(SOCK_ERROR(res))
			return ERR_RECONNECT;
		if(res < 0) return ERR_FAILED;	
		
		mkd = 1; 

		res = ftp_do_cwd(ftp, path);
		if(SOCK_ERROR(res))
			return ERR_RECONNECT;
		if(res < 0) return ERR_FAILED;
	}
	/* return 1 if we successfully created a directory, 0 if everything went allright */
	return mkd;
}

/* try direct cwding. this is not rfc-conform, but works in
 * most cases and usually safes time. if it fails we fall back
 * on the safe method */
/* error-levels: ERR_FAILED, ERR_RECONNECT */
int do_cwd(_fsession * fsession){
	int res;
	/* by unescaping, we get in trouble with certain characters (such as /)
	* thats why this method might fail sometimes. it also fails if the
	* directory does not exist or is a file */
	char * unescaped = cpy(fsession->target_dname);
	char * relative;
	clear_path(unescaped);
	unescape(unescaped);
	if(fsession->ftp->current_directory) clear_path(fsession->ftp->current_directory);
	
	printout(vDEBUG, "previous directory: %s\ttarget: %s\n", fsession->ftp->current_directory, unescaped);
	if(fsession->ftp->current_directory) {
		relative = get_relative_path(fsession->ftp->current_directory, unescaped);
		free(unescaped);
		unescaped = relative;
	}
	
	res = ftp_do_cwd(fsession->ftp, unescaped);
	free(unescaped);
	return res;
}

/* handle the resume_table-> urgs ugly. TODO NRV better idea?
 * TODO NRV recheck this. make it smaller, easier */
void set_resuming(_fsession * fsession) {
	if(fsession->local_fsize < fsession->target_fsize && opt.resume_table.small_large == RESUME_TABLE_UPLOAD) {
		fsession->target_fsize = -1;
		printout(vMORE, _("Remote file size is bigger than local size. Restarting at 0\n"));
	}
	else if(fsession->local_fsize == fsession->target_fsize && fsession->local_fname && opt.resume_table.large_large == RESUME_TABLE_UPLOAD) {
		fsession->target_fsize = -1;
		printout(vMORE, _("Remote file size is equal to local size. Restarting at 0\n"));
	}
	else if(fsession->local_fsize > fsession->target_fsize && opt.resume_table.large_small == RESUME_TABLE_UPLOAD) {
		fsession->target_fsize = -1;
		printout(vMORE, _("Remote file size is smaller than local size. Restarting at 0.\n"));
	}
}

int check_timestamp(_fsession * fsession) {
	int res = ftp_get_modification_time(fsession->ftp, fsession->target_fname, &fsession->target_ftime);
	if(SOCK_ERROR(res)) return res;
	
	/* this is for getting our local ftime in UTC+0 format which ftp-servers
	 * usually issue. add the time-deviation to permit little clock-skews */
	/* TODO USS time_deviation? + or - ? */
	fsession->local_ftime = mktime(gmtime(&fsession->local_ftime)) - opt.time_deviation;
	printout(vDEBUG, "timestamping: local: %d seconds\n"
	                 "             remote: %d seconds; diff: %d\n",
		(int) fsession->local_ftime, (int) fsession->target_ftime,
		(int) (fsession->local_ftime - fsession->target_ftime));
	printout(vDEBUG, "timestamping: local: %s", ctime(&fsession->local_ftime));
	printout(vDEBUG, "              remote: %s", ctime(&fsession->target_ftime));
	if(fsession->target_ftime > fsession->local_ftime) {
		printout(vLESS, "-- Skipping file: %s (remote is newer)\n", fsession->local_fname);
		fsession->done = 1;
		return 1;
	}
	return 0;
}

int open_input_file(_fsession * fsession) {
	int fd;
	int oflags = O_RDONLY;
	char * cmd;
	FILE * pipe;
	
	/* open the input-stream. either file or a pipe */
	if(fsession->local_fname) {
		/* don't know why */
		#if defined(O_BINARY)
		if(fsession->binary == TYPE_I) oflags = oflags | O_BINARY;
		#endif
		
		if((fd=open(fsession->local_fname, oflags)) == -1) {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("Cannot open local source file to read\n"));
			fsession->done = 1;
			return ERR_FAILED;
		}
	} else if(opt.input_pipe) {
		cmd = malloc(
			strlen(opt.input_pipe)
			+ strlen(fsession->user)
			+ (fsession->host->ip ? 15 : strlen(fsession->host->hostname)) + 5 /*port*/
			+ (fsession->target_dname ? strlen(fsession->target_dname) : 0)
			+ strlen(fsession->target_fname)
			+ 18);
		sprintf(cmd, "%s ftp \"%s\" \"%s\" %d \"%s\" \"%s\"",
			opt.input_pipe, fsession->user,
			(fsession->host->ip ? printip((unsigned char *) &fsession->host->ip) : fsession->host->hostname),
			fsession->host->port, fsession->target_dname ? fsession->target_dname : "", fsession->target_fname);
	
		pipe = popen(cmd, "r");
		free(cmd);
		if(pipe == NULL) {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("opening the input-pipe failed: %s\n"), strerror(errno));
			fsession->done = 1;
			return ERR_FAILED;
		}
		fd = fileno(pipe);
		/* TODO USS make the progressbar show, that we don't know the filesize/eta */
		/* this is for other calculation that would fail if we set a to low value. 
		* assume that this will work for now, especially since these >2GB files don't
		* work anyway. use the old-fashioned progress-output which is at least more
		* remote-size independent than the new one. */
		fsession->local_fsize = 2047 * 1024 * 1024;
		opt.barstyle = 0;
	}
	return fd;
}
/* finally this is about actually transmitting the file.
 * putting it through the socket and giving status information to the logfile */
/* TODO NRV do_send() contains a lot of code. maybe too much? */
/* error-levels: ERR_FAILED, get_msg() */
int do_send(_fsession * fsession){
#define DBUFSIZE 1024
	char        databuf[DBUFSIZE];
	int         fd          = open_input_file(fsession);
	int         readbytes   = 0;
	int         res         = 0;
	off_t       transfered_size = 0;
	int         transfered_last = 0;
	unsigned char backupbarstyle = opt.barstyle;
	
	struct wput_timer * timers[2];
	
	char * d                = NULL;
	char * p                = NULL;
	int    convertbytes     = 0;
	char   convertbuf[DBUFSIZE];
	int    crcount          = 0;

	
	res = ftp_establish_data_connection(fsession->ftp);
	if(res < 0) return res;
	
	/* TODO USS make resuming work for ascii-files too */
	if(fsession->binary == TYPE_A) {
		printout(vMORE, _("Disabling resuming due to ascii-mode transfer\n"));
		fsession->target_fsize = -1;
	}
	
	if(fsession->target_fsize > 0) {
		res = ftp_do_rest(fsession->ftp, fsession->target_fsize);
		if(SOCK_ERROR(res)) return res;
		if(res == ERR_FAILED)
			fsession->target_fsize = -1;
	}
	
	while(1) {
		res = ftp_do_stor(fsession->ftp, fsession->target_fname);
		if(res == 1 ) { /* disable resuming */
			if(fsession->target_fsize == -1) {
				res = ERR_FAILED;
				break;
			} else {
				/* just to be sure that it gets resetted... */
				res = ftp_do_rest(fsession->ftp, fsession->target_fsize = -1);
				if(SOCK_ERROR(res)) return res;
			}
		} else if(res == ERR_RETRY && (fsession->retry > 0 || fsession->retry == -1))
			retry_wait(fsession);
		else break;
	}
	if(res == ERR_FAILED)
		fsession->done = 1;
	if(res < 0) return res;
	
	/* we now have to accept the socket (if listening) and close the listening server */
	if( ftp_complete_data_connection(fsession->ftp) == ERR_FAILED) return ERR_FAILED;
	/* -1 indicates that the file does not exist remotely, but now,
	 * after we set resuming, we can again start assuming that remote
	 * file is 0 bytes long (needed for some calculations) */	
	if(fsession->target_fsize == -1) fsession->target_fsize = 0;
	/* initiate progress-output */
	bar_create(fsession);
	
	/* set start times */
	timers[0] = wtimer_alloc();
	timers[1] = wtimer_alloc();
	wtimer_reset(timers[0]);
	wtimer_reset(timers[1]);
	/* prepare resuming */
	if(fsession->target_fsize > 0) {
		/* TODO USS fseek in win (don't know about other OS) does not know 
		 * TODO USS what to do about files >4GB (__int64), so we need ideas
		 * TODO USS here ;) */
		 
		/* maybe try fseeko, fseeko64. huh? what's the standart? 
		 * or we could seek step-wise if size > 2gb */
		 
		/* for now assume that off_t is set to a corresponding value by the OS and therefore
		 * fseek has to use off_t and if it does not work, this is not my problem! */
	
		fseek(fdopen(fd,"r"), fsession->target_fsize, SEEK_SET);
		transfered_size = fsession->target_fsize;
	}
	
	memset(databuf, 0, DBUFSIZE);
	while( (readbytes = read(fd, (char *)databuf, DBUFSIZE)) != 0 ) {    
		if( readbytes == -1 ) {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("local file could not be read: %s\n"), strerror(errno));
			free(timers[0]);
			free(timers[1]);
			return ERR_FAILED;
		}
	
		/* TODO NRV speed_limit should take other reference-data than the average speed... 
		 * TODO NRV maybe average-speed during the last minute or st. similar.
		 * works for now noone complained ;-) */
		if(opt.speed_limit > 0 ) {
			double elapsed_time = wtimer_elapsed(timers[0]);
			while(elapsed_time > 0 && WINCONV(transfered_size - fsession->target_fsize) / (elapsed_time / 1000) > opt.speed_limit) {
				usleep(1000 * 200); /* sleep 0.2 seconds */
				elapsed_time = wtimer_elapsed(timers[0]);
			}
		}
	
		if(fsession->binary == TYPE_A){
			d = databuf;
			p = convertbuf;
			crcount = 0;
			/* TODO NRV ascii mode means more than just CRLF (7-bit), i suppose this
			 * TODO NRV is enough, but maybe someone has time to play around... */
			/* simply replace all \n by \r\n unless there is already an \n */
			while( d < databuf + readbytes){
				while ((p < convertbuf + DBUFSIZE) && (d < databuf + readbytes)){
					if (*d == '\n' && *(d-1) != '\r'){
						*p++ = '\r';
						crcount++;
					}
					*p++ = *d++;
				}
				/* send data converted so far */
				convertbytes = p - convertbuf;
				
				res = socket_write(fsession->ftp->datasock, convertbuf, convertbytes);
				if (res != convertbytes){
					printout(vLESS, _("Error: "));
					printout(vLESS, _("Error encountered during uploading data\n"));
					free(timers[0]);
					free(timers[1]);
					opt.transfered_bytes += transfered_size - fsession->target_fsize;
					res = ftp_do_abor(fsession->ftp);
					if(SOCK_ERROR(res)) return ERR_RECONNECT;
					return ERR_FAILED;
				}
				/* reset convert buffer to proceed */
				p = convertbuf;
			}
			transfered_size += crcount + readbytes;
			transfered_last += crcount + readbytes;
		}
		else {
			transfered_size += readbytes;
			transfered_last += readbytes;
			res = socket_write(fsession->ftp->datasock, databuf, readbytes);
			if(res != readbytes) {
				printout(vLESS, _("Error: "));
				printout(vLESS, _("Error encountered during uploading data (%s)\n"), strerror(errno));
				free(timers[0]);
				free(timers[1]);
				opt.transfered_bytes += transfered_size - fsession->target_fsize;
				res = ftp_do_abor(fsession->ftp);
				if(SOCK_ERROR(res)) return ERR_RECONNECT;
				return ERR_FAILED;
			}
		}
		if(opt.verbose >= vNORMAL) {
			bar_update(fsession, transfered_size, transfered_last, timers[1]);
			transfered_last = 0;
		}
	}
	
	/* TODO USS ok the pipe-handle is missing. so we just close the fd? memory-leak... */
/*	if(!fsession->local_fname && opt.input_pipe)
		pclose(pipe);
	else*/
	if(fd != -1)
		close(fd);
	
	if(fsession->ftp->datasock) {
		socket_close(fsession->ftp->datasock);
		fsession->ftp->datasock = NULL;
	}
	
	/* receive the final message. allow 1xy answers because they might have
	 * been timeouted in do_stor and it's ok if we receive them here */
	printout(vNORMAL, "\n");
	while( (res = ftp_get_msg(fsession->ftp)) == ERR_POSITIVE_PRELIMARY) ;
	
	printout(vNORMAL, "%s (%s) - `%s' [%l]\n\n",
			time_str(),
			fsession->target_fname,
			calculate_transfer_rate(
				wtimer_elapsed(timers[0]), 
				transfered_size - fsession->target_fsize, 0),
			(fsession->local_fname ? fsession->local_fsize : transfered_size));
	
	free(timers[0]);
	free(timers[1]);
	
	opt.transfered_bytes += transfered_size - fsession->target_fsize;
	opt.transfered++;
	
	if(transfered_size == fsession->local_fsize || fsession->binary == TYPE_A || !fsession->local_fname)
		fsession->done = 1;
	
	if(res == ERR_RETRY) return ERR_RETRY;
	if(FTP_ERROR(res))   return ERR_FAILED;
	if(SOCK_ERROR(res)) return res;
	
	if( fsession->local_fname &&
		(transfered_size == fsession->local_fsize || fsession->binary == TYPE_A) 
		&& opt.unlink) {
			printout(vMORE, _("Removing source file `%s'\n"), fsession->local_fname);
			unlink(fsession->local_fname);
	}
	
	opt.barstyle = backupbarstyle;
	
	return 0;
}
#define SOCKET_RETRY \
	if(SOCK_ERROR(res)) {\
		retry_wait(fsession);\
		ftp_quit(fsession->ftp);\
		fsession->ftp = ftp = NULL;\
		continue;\
	}


/* error-levels: 0 (success), -1 (failed), -2 (skipped) */
int fsession_transmit_file(_fsession * fsession, ftp_con * ftp) {
	int res = 0;
	/* we don't do any GUI interactive stuff, so we can afford a "simpler" flow
	* of command sequence */
	
	/* if we were unable to login / cwd / mkdir into a directory on the same 
	* host:port/(dir) skip this file */
	if(skiplist_find_entry(fsession->host->ip, fsession->host->hostname, fsession->host->port, fsession->user,
			fsession->pass, fsession->target_dname))
	{
		printout(vLESS, _("-- Skipping file: `%s'\n"), fsession->local_fname);
		return ERR_FAILED;
	}
	
	printout(vLESS,
			"--%s-- `%s'\n"
			"    => ftp://%s:xxxxx@%s:%d/%s%s%s\n",
			time_str(),
			fsession->local_fname,
			fsession->user,
			fsession->host->ip ? printip((unsigned char *) &fsession->host->ip) : fsession->host->hostname,
			fsession->host->port,
			fsession->target_dname,
			fsession->target_dname ? "/" : "", 
			fsession->target_fname);

  while(!fsession->done && ( fsession->retry > 0 || fsession->retry == -1) ){
	printout(vDEBUG, "starting again\n");
    /* ftp is the last ftp-connection. so we check whether both hosts match. 
	 * if not the old connection is closed and a new one is created */
	if(ftp) {
		if(ftp->host->ip       == fsession->host->ip && 
		   ftp->host->port     == fsession->host->port && 
		   SAVE_STRCMP(ftp->host->hostname, fsession->host->hostname))
			fsession->ftp = ftp;
		else
			ftp_quit(ftp);
	}
	if(!fsession->ftp)
		fsession->ftp = ftp_new(ftp_new_host(fsession->host->ip, fsession->host->hostname, fsession->host->port), opt.tls);
	
	/* if there is already an established connection skip the connecting procedure */
	if(!fsession->ftp->sock) {
		if(ftp_connect(fsession->ftp, &opt.ps) == ERR_FAILED) {
			retry_wait(fsession);
			ftp_quit(fsession->ftp);
			fsession->ftp = ftp = NULL;
			continue;
		}
		/* set the portmode default value to the global setting */
		fsession->ftp->portmode = opt.portmode;
		fsession->ftp->bindaddr = opt.bindaddr;
	}

	/* log in. ftp_do_login checks on its own whether the user is already logged in */
	res = ftp_login(fsession->ftp, fsession->user, fsession->pass);
	/* in case that the connection broke down or st. ... */
	SOCKET_RETRY;
	
	if( res < 0 ) {
		printout(vLESS, _("Skipping all files from this account...\n"));
		opt.skipdlist = skiplist_add_entry(opt.skipdlist, fsession->host->ip, 
			fsession->host->hostname ? cpy(fsession->host->hostname) : NULL,
			fsession->host->port,	cpy(fsession->user), 
			fsession->pass ? cpy(fsession->pass) : NULL, NULL);
		ftp_do_quit(fsession->ftp);
		fsession->ftp = NULL;
		return ERR_FAILED;
	}

	/* removed this feature, since it will be replace by 
	 * some scripting feature one day. haha "one day" */
	/* if(fsession.sitecmd) do_sitecmd(fsession.sitecmd); */

	/* since we transfer each file in a seperate session,
	 * we must compare the pathnames too */
	
	/* remove things like dir1/../ from the path */
	if(fsession->target_dname)
		clear_path(fsession->target_dname);
	
	if(fsession->target_dname && (
		fsession->ftp->needcwd || (fsession->ftp->current_directory &&
		strcmp(fsession->ftp->current_directory, fsession->target_dname))))
	{
		res = do_cwd(fsession);
		if( res == ERR_FAILED ) {
			/* cwd for each directory in path */
			res = long_do_cwd(fsession);
			if(res == ERR_FAILED) {
				/* TODO USS the current_directory might have changed, so it would be
				 * TODO USS wise to set fsession->ftp->current_directory to the actual one */
				fsession->ftp->needcwd = 1;
				printout(vLESS, _("Failed to change to target directory. Skipping this file/dir.\n"));
				return ERR_FAILED;
			}
		}
		SOCKET_RETRY;
		
		/* on success mark this dir */
		fsession->ftp->needcwd = 0;
		if(fsession->ftp->current_directory) 
			free(fsession->ftp->current_directory);
		fsession->ftp->current_directory = cpy(fsession->target_dname);
	}

    /* on most ftps we have to say PASV or PORT before typing REST n. 
     * So i assume that it's best to _only_ SIZE here and do REST in do_send() */
    /* we don't need to SIZE for input-pipes, since we don't know the local file-size anyway */
	/* don't size if we are going to upload anyway */
	if(fsession->resume_table->small_large == RESUME_TABLE_UPLOAD &&
	   fsession->resume_table->large_large == RESUME_TABLE_UPLOAD &&
	   fsession->resume_table->large_small == RESUME_TABLE_UPLOAD)
		fsession->target_fsize = -1;
	else
    	if(fsession->local_fname) {
			res = ftp_get_filesize(fsession->ftp, fsession->target_fname, &fsession->target_fsize);
    		if(res == ERR_FAILED) fsession->target_fsize = -1;
			SOCKET_RETRY;
		}
	
	printout(vDEBUG, "local_fsize: %d\ntarget_fsize: %d\n",
		(int) fsession->local_fsize,
		(int) fsession->target_fsize);
    printout(vDEBUG, "resume_table: %d,%d,%d\n", fsession->resume_table->small_large,
		fsession->resume_table->large_large, fsession->resume_table->large_small);
	/* check whether this file is to be skipped */
	if( fsession->local_fname && ( /* if we have an input-pipe, both sizes are 0. ignore it */
	   (fsession->local_fsize < fsession->target_fsize && fsession->resume_table->small_large == RESUME_TABLE_SKIP) ||
	   (fsession->local_fsize == fsession->target_fsize && fsession->resume_table->large_large == RESUME_TABLE_SKIP) ||
	   (fsession->local_fsize  > fsession->target_fsize && fsession->resume_table->large_small == RESUME_TABLE_SKIP)))
	{
		res = ERR_SKIP;
		fsession->done = 1;
		printout(vMORE, _("Skipping this file due to resume/upload/skip rules.\n"));
		printout(vLESS, _("-- Skipping file: %s\n"), fsession->local_fname);
		break;
	}
	/* figure out the resume/upload/skip rules and probably set the remote filesize to -1 */
	set_resuming(fsession);
	
	/* check timestamp and skip the file if whished */
	if(opt.timestamping)
		if(check_timestamp(fsession)) {
			res = ERR_SKIP;
			fsession->done = 1;
			break;
		}

	/* set the filemode based on the extension unless it has been specified by */	
	if(fsession->binary == TYPE_UNDEFINED)
		fsession->binary = get_filemode(fsession->target_fname);

	res = ftp_set_type(fsession->ftp, fsession->binary);
	SOCKET_RETRY;
	
	if(res == ERR_FAILED)
		printout(vMORE, _("Unable to set transfer mode. Assuming binary\n"));

	/* transmit the file and retry if requested */
    while((res = do_send(fsession)) == ERR_RETRY) {
		retry_wait(fsession);
		if(!( fsession->retry > 0 || fsession->retry == -1)) {
			res = ERR_FAILED;
			break;
		}
	}
	SOCKET_RETRY;
	
	if(res == ERR_FAILED) {
		printout(vLESS, _("Send Failed. "));
		if(fsession->done) {
			printout(vLESS, _("Skipping this file\n"));
			break;
		} else {
			retry_wait(fsession);
			/* if a dir gets deleted while we are still in it, we fail
			* and need to re_cwd */
			fsession->ftp->needcwd = 1;
	    	continue;
		}
    } 
	/* TODO USS is there any case when do_send fails, that the whole directory 
	 * TODO USS is to be skipped? */
	/*else if( res == -2) {
        printout(vLESS, "Send Failed. Skipping all files from this directory\n");
        opt.skipdlist = skiplist_add_entry(opt.skipdlist,
            fsession->ip,
            fsession->port, 
            cpy(fsession->user), cpy(fsession->pass),
            (fsession->target_dname ? cpy(fsession->target_dname) : NULL));
        break;
    }*/
        
  } /* while */

	/* TODO NRV wait only if this is _not_ the last file */
	/* wait opt.wait seconds or if random_wait is enabled wait
	 * st. between 0 and 2*opt.wait 10th-seconds (average would be opt.wait) */
	if(opt.wait) {
		usleep( (opt.random_wait ? 2 * (float) rand() / 0x7fffffff : 1)
			* 1000 * 100 * opt.wait);
	}
	
	return res;
}

/* this function tries to parse a ftp-url that should look like
 * ftp://[user[:password]@]hostname[:port][/[remote_path/][remote_file]] */
int parse_url(_fsession * fsession, char *url) {
	char * d;
	char * host = NULL;
	char * path = NULL;
	
	/* while unescaping, we write to url, but we may not modify it, because it
	 * might be used later on and this leads to undefined behavior. but we also may
	 * not loose the base_pointer for freeing it afterwards, so get a backup */
	url = cpy(url+6);
	d = strchr(url, '/');
	if(d) *d = 0, path = d + 1;
	/* using strrchr to find the last @. therefore usernames containing an @
	 * should be valid as well */
	d = strrchr(url, '@');
	if(d) *d = 0, host = d + 1;
	else  host = url;
	
	/* username / password */
#if 1 //__MSTC__, Dennis	
	FILE *fp = NULL;
	char str[64]={"\0"};
	char str1[64]={"\0"};	
	fp = fopen("/var/wput","r");
	if (fp!=NULL)
	{
		fgets(str,64,fp);
		fgets(str1,64,fp);
		fsession->user= cpy(str);
		fsession->pass = cpy(str1);
		fclose(fp);
	}else{
#endif
	if(d) {
		d = strchr(url, ':');
		if(d) {
			*d = 0;
			fsession->user = cpy(unescape(url));
			fsession->pass = cpy(unescape(d+1));
		} else
			fsession->user = cpy(unescape(url));
	}
#if 1 //__MSTC__, Dennis
	}
#endif //__MSTC__, Dennis
	fsession->host = ftp_new_host(0,NULL,21);
	
	/* port */
	d = strchr(host, ':');
	if(d)
		*d = 0,
		fsession->host->port = atoi(d + 1);
	
	/* hostname */
	if( get_ip_addr(host, &fsession->host->ip) == -1) {
		if(opt.ps.type != PROXY_OFF) {
			fsession->host->hostname = cpy(host);
			printout(vMORE, _("Warning: "));
			printout(vMORE, _("`%s' could not be resolved. "), host);
			printout(vMORE, _("Assuming the proxy to do the task.\n"));
		} else {
			printout(vMORE, _("Error: "));
			printout(vMORE, _("`%s' could not be resolved. "), host);
			printout(vLESS, _("Skipping this URL.\n"));
			free(url);
			return ERR_FAILED;
		}
	}
	
	/* look up the password list for an entry for this host and user */
	if(!fsession->pass) {
		password_list * P = password_list_find(opt.pl, host, fsession->user);
		if(P) {
			if(!fsession->user) fsession->user = cpy(P->user);
			fsession->pass = cpy(P->pass);
		} else if(!fsession->user) {
			fsession->user = cpy("anonymous");
			fsession->pass = cpy(opt.email_address);
		}
	}
	
	/* path and filename */
	if(!path) {
		free(url);
		return 0;
	}
	
	/* last '/' is where the filename starts */
	d = strrchr(path, '/');
	if(d) 
		*d = 0,
		fsession->target_dname = cpy(path);
	else
		d = path-1;
		
	if(*++d) {
		if(strchr(d, '#') || strchr(d, '?')) {
			printout(vNORMAL, _("Warning: "));
			printout(vNORMAL, _("URL: # or ? functions unimplemented. Assuming they are part of the filename.\n"));
		}
		fsession->target_fname = cpy(unescape(d));
	}
	free(url);
	return 0;
}
