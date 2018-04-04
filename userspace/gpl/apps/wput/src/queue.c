/* input-queue and fsession functions */

/* Author: Hagen Fritsch <fritsch+wput-src@in.tum.de>
   (C) 2002-2006 by Hagen Fritsch
    
   This file is part of wput.

   This programm is free software; you can redistribute it and/or
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
   
/* Files and URLs can be read from both commandline and/or
 * inputfile/-pipe. So we might know all files at once but get the URLs
 * later or reversed.
 * So we take a queue where all files and URLs get in and unless sort is
 * enabled, as soon as we get the first complete file/URL pair, we'll 
 * start the upload.
 * Problems encountered here:
 *  o One URL may take multiple files (e.g. directory)
 *  o We sometimes don't need a filename if we got the URL
 * */
#include "wput.h"
#include "_queue.h"
#include "utils.h"
#include "progress.h"
#include "ftp.h"

typedef struct input_queue {
  char * url;
  char * file;
  struct input_queue * next;
} _queue;

_queue * queue_entry_point = NULL;

void printqueue(_queue * K) {
    if(K == NULL) return;
    printout(vDEBUG, "File: `%s'\nURL : `%s'\n", K->file, K->url);
    printqueue(K->next);
}

void queue_add_file(char * filename) {
  _queue * K = queue_entry_point;

  printout(vDEBUG, "Added file `%s' to queue.\n", filename);

  while(K != NULL && K->file != NULL) K = K->next;
  /* queue entry with filename */
  if(K != NULL) K->file = filename;
  else queue_add_entry(filename, NULL);

}

void queue_add_url(char * url) {
  _queue * K = queue_entry_point;

  printout(vDEBUG, "Added URL `%s' to queue.\n", url);

  /* save the url, for maybe url-less files that might belong to it */
  /* we do it here since we only want 'real' urls to be known and not
   * generated urls (e.g. by the recur-dir-function) */
  if(opt.last_url) free(opt.last_url);
  opt.last_url = cpy(url);

  while(K != NULL && K->url != NULL) K = K->next;
  /* queue entry with URL */
  if(K != NULL) K->url = url;
  else queue_add_entry(NULL, url);
}

void queue_add_entry(char * file, char * url) {
  _queue * K = queue_entry_point;
  _queue * M = malloc(sizeof(_queue));
  
  M->url  = url;
  M->file = file;
  M->next = NULL;
  
  if(K == NULL) queue_entry_point = M;
  else {
    K = queue_entry_point;
    while(K->next != NULL) K = K->next;
    K->next = M;
  }
}
/* we only process complete entries here. lonely urls are process after everything
 * has been read except when force is set to 1 */
void queue_process(int force) {
	_queue * P;
	int res;
	
	printout(vDEBUG, "processing queue:\n");
	if(opt.verbose >= vDEBUG)
		printqueue(queue_entry_point);
	
	if(queue_entry_point == NULL) return;
	while(queue_entry_point != NULL && queue_entry_point->url != NULL && (queue_entry_point->file != NULL || force)) {
		_fsession * F = build_fsession(queue_entry_point->file, queue_entry_point->url);
		if(F && F != (void *) -2) {
			if(!opt.sorturls) {
				res = fsession_transmit_file(F, opt.curftp);
				if(res == -1)      opt.failed++;
				else if(res == -2) opt.skipped++;
				if(F->ftp)
				  opt.curftp = F->ftp;
				free_fsession(F);
			} else
				fsession_queue_entry_point = fsession_insert(F, fsession_queue_entry_point);
		} else
			printout(vDEBUG, "ignoring unbuild fsession\n");
		P = queue_entry_point;
		queue_entry_point = queue_entry_point->next;
	
		/* the file is free()d by build_fsession, the url is our task */
		free(P->url);
		free(P); 
	}
}
/* we just make all remaining filenames have the last known url */
void process_missing(void) {
    _queue * K = queue_entry_point;
    if(opt.last_url) {
        while(K != NULL) {
            if(!K->url) 
                K->url = cpy(opt.last_url);
            K = K->next;
        }
    }
    queue_process(1);
}
/*
_fsession * fsession_queue_add(_fsession * F, _fsession * Q) {
  if(Q == NULL) return F;
  Q->next = fsession_queue_add(F, Q->next);
  return Q;
}
*/
/* this function takes a directory as input and adds
   all its files to the upload queue. */
/* url must end with a slash! */
int queue_add_dir(char * dname, char * url, _fsession * fsession){
	/* here we need to use to mainly different implementations
	* in windows we use FindFirstFile -> FindNextFile -> FindClose
	* in *nix, we simply opendir */
	/* there is less common in this function maybe there should be
	 * two different functions... */
	
	char * fname = 0;
		
#ifdef WIN32
	
	int pathlen = strlen(dname)+3; //so we don't need to recalculate everytime (+3== '\\*\0');
	char tmpbuf[MAX_PATH];
	WIN32_FIND_DATA statbuf;
	HANDLE hSearch;
	#define cFileName statbuf.cFileName
	
#else
	
	char * tmpbuf;
	
	struct stat statbuf;
	struct dirent * dent;
	DIR * hSearch;
	#define cFileName dent->d_name

	tmpbuf = (char *) malloc(strlen(dname)+2); //take space for this strange slash that we need to append st.
#endif
	strcpy(tmpbuf, dname);
	/*printout(vDEBUG, "recur-ftp: %s => %s\n", dname, url);*/
#if 0 /* this code does not make any sense */
	/* we have to "replace" the beginning of the directory, if there
	* is no slash at the end of the input_url */
	if(url_lastchar != '/') {
		if(fsession->local_fname) dname += strlen(fsession->local_fname);
		printout(vDEBUG, "dname: %d\n", *dname);
		if(*dname != 0 && *dname != dirsep) dname--;
	}
#endif
#ifdef WIN32
	/* for searching, we need to append * at each path, and since it does
	* not end with \, a backslash needs also to be appended */
	/* TODO USS WIN32: sure, it never can end with a backslash? */
	printout(vDEBUG, "tmpbuf: %s (has a backslash?)\n", tmpbuf);
	tmpbuf[pathlen-3] = '\\';
	tmpbuf[pathlen-2] = '*';
	tmpbuf[pathlen-1] = 0;
#else
	/* Force this nervous-making slash to be at the end of dname */
	if(tmpbuf[strlen(tmpbuf)-1] != dirsep)
		strcat(tmpbuf, "/");
#endif

#ifdef WIN32
	if( (hSearch = FindFirstFile(tmpbuf, &statbuf)) ) {
		do {
#else
	if( (hSearch = opendir(tmpbuf)) ) {
		while( (dent = readdir(hSearch)) != 0x0){
#endif

			printout(vDEBUG, "Dir entry name: %s\n", cFileName);
			/* skip navigation-links */
			if(!strcmp(cFileName, ".") || !strcmp(cFileName, "..")) continue;
			
			if(fname) free(fname);
			/* concat path and file */
#ifdef WIN32 
			/* we have that '*' there so we can simply write our
						null-char there and need one byte less */
			fname = (char *) malloc(strlen(tmpbuf) + strlen(cFileName));
			strcpy(fname, tmpbuf);
			strcpy(fname + pathlen-2, cFileName);
#else
			fname = (char *) malloc(strlen(tmpbuf) + strlen(cFileName)+1);
			strcpy(fname, tmpbuf);
			strcpy(fname + strlen(tmpbuf), cFileName);
#endif
		
			/* TODO NRV WIN32 symlinks don't exist in windows, do they? should
			 * TODO NRV WIN32 we upload these strange lnk-files using normal
			 * TODO NRV WIN32 binary mode? or do they need to be dereferenced? */
#ifdef WIN32
			if(! (statbuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
#else
			if( stat(fname, &statbuf) == 0){
			  if ( S_ISREG(statbuf.st_mode) ){
#endif
				/* the url remains unchanged. it will get completed later... */
				printout(vDEBUG, "fname: %s (url: %s)\n", fname, url);
				//printout(vDEBUG, "target_fname: %s\n", cFileName);
				//printout(vDEBUG, "dname: %s\n", dname);
				queue_add_entry(cpy(fname), cpy(url));
			}else
#ifndef WIN32
			if ( S_ISDIR(statbuf.st_mode) )
#endif
				queue_add_dir(fname, url, fsession);
#ifdef WIN32
		} while( FindNextFile(hSearch, &statbuf) );
		FindClose(hSearch);
#else  /* !WIN32 */
	  }else {
	  	printout(vLESS, _("Warning: "));
		printout(vLESS, _("Error encountered but ignored during stat of `%s'.\n"), fname);
	  }
    } /* while */
    closedir(hSearch);
#endif
  }else {
    printout(vLESS, _("Warning: "));
    printout(vLESS, _("Error encountered but ignored during opendir of `%s'.\n"), fname);
  }
  if(fname) free(fname);
#ifndef WIN32 /* WIN uses a static one */
  free(tmpbuf);
#endif
  return 0;
}
int fsession_compare(_fsession * A, _fsession * B) {
	int a;
    /* compare by weight. ip, hostname, port, user, pass, directory, file */
    if(A->host->ip > B->host->ip) return 1;
    if(A->host->ip < B->host->ip) return 0;
	if(A->host->hostname && B->host->hostname) {
		a = strcmp(A->host->hostname, B->host->hostname);
		if(a > 0) return 1;
		if(a < 0) return 0;
	}
    if(A->host->port > B->host->port) return 1;
    if(A->host->port < B->host->port) return 0;
    if(A->user && B->user) {
		a = strcmp(A->user, B->user);
        if(a > 0) return 1;
        if(a < 0) return 0;
    }
    if(A->pass && B->pass) {
		a = strcmp(A->pass, B->pass);
        if(a > 0) return 1;
        if(a < 0) return 0;
    }
    if(A->target_dname && B->target_dname) {
		a = strcmp(A->target_dname, B->target_dname);
        if(a > 0) return 1;
        if(a < 0) return 0;
    }
    if(A->target_fname && B->target_fname) {
		a = strcmp(A->target_fname, B->target_fname);
        if(a > 0) return 1;
        if(a < 0) return 0;
    }
	printout(vLESS, _("Warning: "));
    printout(vLESS, _("Seems as though there are two equivalent entries to upload.\n"));
    return 0;
}
/* this inserts an fsession K into the queue. the resulting queue is
 * sorted */
_fsession * fsession_insert(_fsession * K, _fsession * F) {
    if(F == NULL)
        return K;
    if(fsession_compare(F, K)) {
        K->next = F;
        return K;
    }
    F->next = fsession_insert(K, F->next);
    return F;
}
/* sorting works using simple insert-sort *
void fsession_sort(void) {
    _fsession * F = fsession_queue_entry_point;
    _fsession * K;
    fsession_queue_entry_point = NULL;
    while(F != NULL) {
        K = F->next;
        F->next = NULL;
        fsession_queue_entry_point = fsession_insert_sort(fsession_queue_entry_point, F);
        F = K;
    }
}
*/
    
void free_fsession(_fsession * F) {
    fsession_queue_entry_point = F->next;
    ftp_free_host(F->host);
    if(F->local_fname)  free(F->local_fname);
    if(F->target_dname) free(F->target_dname);
    if(F->target_fname) free(F->target_fname);
    if(F->user)         free(F->pass);
    if(F->pass)         free(F->user);
    free(F);
}
/* build fsession.
 * the fsession is supposed to know about anything required to transfer
 * the particular file. */
 
_fsession * build_fsession(char * file, char * url) {
	_fsession * fsession = malloc(sizeof(_fsession));
	struct stat statbuf;

	/* init fsession */
	memset(fsession, 0, sizeof(_fsession));
	/* default options, by global settings */
	fsession->binary       = opt.binary;
	fsession->retry        = opt.retry;
	fsession->resume_table =&opt.resume_table;
	
	if( parse_url(fsession, url) == ERR_FAILED) {
		printout(vLESS, _("Error: "));
		printout(vLESS, _("the url `%s' could not be parsed\n"), url);
		if(file) free(file);
		free_fsession(fsession);
		printout(vDEBUG, "fsession free()d\n");
		return NULL;
	}
	
	printout(vDEBUG, "PRE_GUESS: local_file: %s\n"
                     "remote_path: %s\tremote_file: %s (%x)\n", file,
                     fsession->target_dname, fsession->target_fname, fsession->target_fname);

	if(!file) {
		/* challenging task. a remote file has not been specified, so we try
		 * finding it from the url. */
		char * tmp;
		/* TODO NRV if we got neither a remote dir nor a remote filename looking
		 * TODO NRV in a dir named like hostname for the files might be a good idea ;)
		 * but today we take the local dir as input-dir */
		if(!fsession->target_dname && !fsession->target_fname) {
			printout(vLESS, _("Warning: "));
			printout(vLESS, _("Neither a remote location nor a local filename "
				"has been specified. Assuming you want to upload the "
				"current working directory to the remote server.\n"));
			file = cpy(".");
		} else {
			/* compute the location and try to find a local file of it */
			tmp = file = (char *) malloc(
				(fsession->target_dname ? strlen(fsession->target_dname) : 0) + 1 +
				(fsession->target_fname ? strlen(fsession->target_fname) : 0) + 1);
			file[0] = 0;
			printout(vDEBUG, "computing local_fname: %s\n", tmp);
			if(fsession->target_dname) {
				strcpy(file, fsession->target_dname);
				strcat(file, "/");
			}
			if(fsession->target_fname) strcat(file, fsession->target_fname);
			
			/* not the best solution, but someone has to do it anyway... */
			unescape(file);

#ifdef WIN32
			/* in windows we now have to replace each '/' with a '\' */
			do if(*file == '/') *file = dirsep;
			while(*file++);
			if(*(file-=2) == dirsep) *file = 0;
			file = tmp;
#endif  
			printout(vDEBUG, "Trying to stat '%s'... ", file);
			//file = strchr(file, dirsep);
			while( stat(file, &statbuf) != 0 ) {
				file = strchr(file, dirsep);
				if(!file) {
					/* if we have no remote filename, lets assume the current working
					   directory is to be stored in the remote location. */
					if(!fsession->target_fname) {
						printout(vNORMAL, _("Warning: "));
						printout(vNORMAL, _("No local file specified and no file found from URL.\n"
							"Assuming the current working directory is to be uploaded to "
							"the remote server.\n"));
						file = ".";
						break;
					} else {
						file = "";
						break;
					}
				} else
					file++; /* we don't want a leading dirsep */
				printout(vDEBUG, "failed! (errno: %d)\nTrying to stat '%s'... ", errno, file);
			}
			file = cpy(file);
			printout(vDEBUG, "file: %s, url: %s\n", file, url);
			if( S_ISDIR(statbuf.st_mode) ) {
				printout(vDEBUG, "IS_DIR! ");
				url[strlen(url)-strlen(file)] = 0;
				// - (url[strlen(url)-1] == '/' ? 1 : 0)
			}
			printout(vDEBUG, "done. Found '%s' -> %s\n", file, url);
			free(tmp);
		}
	}
   /* remove the trailing slash if existant */
#ifdef WIN32
	if( file[strlen(file)-1] == dirsep )
		file[strlen(file)-1] = 0;
#endif
	
	if(stat(file, &statbuf) != 0) {
		if(opt.input_pipe) {
			/* TODO NRV is this message really necessary? */
			printout(vMORE, _("Warning: "));
			printout(vMORE, _("File `%s' does not exist. Assuming you supply its input using the -I flag.\n"), file);
			fsession->local_fname = NULL;
			if(!fsession->target_fname) {
				printout(vNORMAL, "TODO USS this might be buggy. Do we know, where to upload?\n");
				fsession->target_fname = basename(file);
			}
			free(file);
			return fsession;
		} else {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("File `%s' does not exist. Don't know what to do about this URL.\n"), file);
			free(file);
			free_fsession(fsession);
			return NULL;
		}
    } else
	/* if file is a directory, we add all its entries to the queue
	 * and finish building this fsession */
	if( S_ISDIR(statbuf.st_mode) ) {
		queue_add_dir(file, url, fsession);
		printout(vDEBUG, "directory added successful\n");
		free_fsession(fsession);
		printout(vDEBUG, "fsession free()d\n");
		free(file);
		return (void *) -2;
	}
    
	fsession->local_fname = file;
	file = snip_basename(file);

	fsession->local_fsize = statbuf.st_size;
	/* assuming that remote_ftps do normal ls, they show the mtime.
	* utc is computed when actually comparing the dates... */
	fsession->local_ftime = statbuf.st_mtime;

	if(!fsession->target_fname && strchr(file, dirsep)) {
		int slashlen = strrchr(file, dirsep) - file;
		if(fsession->target_dname) {
			fsession->target_dname = realloc(fsession->target_dname,
				strlen(fsession->target_dname) + 1 + 
				slashlen + 1);
			strcat(fsession->target_dname, "/");
		} else {
			fsession->target_dname = malloc(slashlen + 1);
			*fsession->target_dname = 0;
		}
#ifdef WIN32
		{
			char * tmp = file+slashlen;
			while(tmp-- != file) if(*tmp == '\\') *tmp = '/';
		}
#endif
		strncat(fsession->target_dname, file, slashlen);
		fsession->target_fname = cpy(basename(file));
	} else if(!fsession->target_fname)
		fsession->target_fname = cpy(file);
	
	printout(vDEBUG, "POST_GUESS: local_file: %s\n"
		"remote_path: %s\tremote_file: %s\n", fsession->local_fname,
		fsession->target_dname, fsession->target_fname);
	/* TODO USS is there anything to do (initialize) before proceeding with upload? */
	return fsession;
}

/* ******************** *
 * routines for managing the skipd-linked list */

/* add a skip entry for the particular host and location */
void makeskip(_fsession * fsession, char * tmp) {
#if 0
	char * ptr = strtok(0x0, "/");
	int    len = ((ptr != 0x0) ? ptr-tmp : strlen(fsession->target_dname));
	char * skipdname = malloc(len+1);
	printout(vDEBUG, "makeskip: %s (%s)\n", tmp, ptr);
	strncpy(skipdname, fsession->target_dname, len);
	skipdname[len] = 0;
	printout(vDEBUG, "len: %d -> path: %s (ptr: %s) => skipd: %s\n", ptr-tmp, tmp, ptr, skipdname);
#endif
	opt.skipdlist = skiplist_add_entry(opt.skipdlist, fsession->host->ip,
		fsession->host->hostname ? cpy(fsession->host->hostname) : NULL,  fsession->host->port,
		cpy(fsession->user), fsession->pass ? cpy(fsession->pass) : NULL, cpy(tmp));
}
 
void skiplist_free(skipd_list * K) {
    skipd_list * N;
    if(K == NULL) return;
    N = K->next;
	if(K->host) free(K->host);
    if(K->dir)  free(K->dir);
    if(K->user) free(K->user);
    if(K->pass) free(K->pass);
    free(K);
    skiplist_free(N);
}
skipd_list * skiplist_add_entry(skipd_list * K, int ip, char * host, unsigned short int port, char * user, char * pass, char * dir) {
    if(K == NULL) {
        skipd_list * N = malloc(sizeof(skipd_list));
		N->ip	= ip;
		N->host = host;
		N->port = port;
		N->user = user;
		N->pass = pass;
		N->dir  = dir;
		N->next = NULL;

        printout(vDEBUG, "Added skip_entry ftp://%s:%s@%s:%d/%s\n", 
            user, pass, ip ? printip((unsigned char *) &ip) : host, port, dir);

		return N;
	}
	K->next = skiplist_add_entry(K->next, ip, host, port, user, pass, dir);
	return K;
}
/* TODO USS check whether all occurable skip_case are handled right */
int skiplist_find_entry(int ip, char * host, unsigned short int port, char * user, char * pass, char * dir) {
	skipd_list * K = opt.skipdlist;
	printout(vDEBUG, "Searching for skip_entry ftp://%s:%s@%s:%d/%s\n",
        user, pass, ip ? printip((unsigned char *) &ip) : host, port, dir);
	while( K != NULL )
	{
		printout(vDEBUG, "Checking skip_entry ftp://%s:%s@%s:%d/%s\n", 
            K->user, K->pass, ip ? printip((unsigned char *) &K->ip) : host, K->port, K->dir);
		
		if(((K->ip	== ip && ip != 0) || 
			 (K->host && host && !strcmp(K->host, host))) &&
			K->port == port         &&
			!strcmp(K->user, user)  &&
			(pass   == K->pass      || (K->pass && pass && !strcmp(K->pass, pass))  ) &&
			(K->dir == NULL         || (dir && !strncmp(K->dir, dir, strlen(K->dir))) ) )
				break;
		K = K->next;
	}
	if(K == NULL) return 0;
	return 1;
}
/* ******************** *
 * routines for managing the password-file-linked list */
password_list * password_list_add(password_list * K, char * host, char * user, char * pass) {
	if(K == NULL) {
		K = malloc(sizeof(password_list));
		K->host = host;
		K->user = user;
		K->pass = pass;
		K->next = NULL;
	} else
		K->next = password_list_add(K->next, host, user, pass);
	return K;
}
/* search for an entry that matches host and user. if none is found, try finding a 
 * match for host only */
/* user can be NULL */
password_list * password_list_find(password_list * K, char * host, char * user) {
	password_list * R = NULL;
	if(!K) return R;
	if(!strcmp(K->host, host) && (!user || !strcmp(K->user, user)))
		return K;
	R = password_list_find(K->next, host, user);
	if(!strcmp(K->host, host) && !R)
		return K;
	return R;
}
void password_list_free(password_list * K) {
	if(K->next) password_list_free(K->next);
	free(K->host);
	free(K->user);
	free(K->pass);
	free(K);
}
