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

/* this file contains library procedures for the ftp-protocol */
#include "ftplib.h"
#include "utils.h"
#include <string.h>
#ifndef WIN32
#  include <netinet/in.h>
#endif

#define ISDIGIT(x) ((x) >= '0' && (x) <= '9')

/* =================================== *
 * ===== basic (de)constructors ====== *
 * =================================== */
 
host_t * ftp_new_host(unsigned ip, char * hostname, unsigned short port) {
	host_t *  h = malloc(sizeof(host_t));
	h->ip       = ip;
	h->hostname = hostname;
	h->port     = port;
	return h;
}

ftp_con * ftp_new(host_t * host, int secure) {
	ftp_con * self = malloc(sizeof(ftp_con));
	memset(self, 0, sizeof(ftp_con));
	self->host   = host;
	self->secure = secure;
	self->sbuf   = malloc(82);
	self->sbuflen= 82;
	return self;
}

void ftp_free_host(host_t * host) {
	if(host->hostname) free(host->hostname);
	free(host);
}

void ftp_quit(ftp_con * self) {
	ftp_free_host(self->host);
	ftp_do_quit(self);
	if(self->current_directory)  free(self->current_directory);
	if(self->r.reply)            free(self->r.reply);
	if(self->user)               free(self->user);
	if(self->pass)               free(self->pass);
	self->current_directory = self->r.reply = NULL;
	free(self->sbuf);
	/* TODO IMP */
	ftp_fileinfo_free(self);
	free(self);
}

/* =================================== *
 * ======= basic communication ======= *
 * =================================== */
 
/* read a line from socket and do some basic parsing, resulting in special
 * error-levels: ERR_RECONNECT, ERR_TIMEOUT, ERR_RETRY, ERR_POSITIVE_PRELIMARY */
 
 /* rfc-states that a reply might look like this:
	123-First line
	Second line
	 234 A line beginning with numbers
	123 The last line
 * numbers must be padded and if other lines follow they do not need to start
 * with XXX-. the last line contains the code again and should be considered
 * relevant. the others are user-information printed on vNOMRAL (or vMORE?) */
int ftp_get_msg(ftp_con * self) {
	char * msg = socket_read_line(self->sock);
	static int multi_line = 0;
	if(self->r.reply) {
		free(self->r.reply);
		self->r.reply = NULL;
	}
	if(!msg) {
		printout(vLESS, _("Receive-Error: Connection broke down.\n"));
		return ERR_RECONNECT;
	}
	if(msg == (char *) ERR_TIMEOUT)
		return ERR_TIMEOUT;
	if(strlen(msg) < 4 || !ISDIGIT(msg[0]) || !ISDIGIT(msg[1]) || !ISDIGIT(msg[2])) {
		if(multi_line) {
			printout(vMORE, "# %s\n", msg);
			free(msg);
			return ftp_get_msg(self);
		}
		printout(vLESS, _("Receive-Error: Invalid FTP-answer (%d bytes): %s\n"), strlen(msg), msg);
		free(msg);
		printout(vLESS, _("Reconnecting to be sure, nothing went wrong\n"));
		return ERR_RECONNECT;
	}
	if(msg[3] == '-') {
		/* hyphened lines indicate that there is another reply line coming
		 * print it out to anyone who is interested and go on walking */
		multi_line = 1;
		printout(vMORE, "# %s\n", msg+4);
		free(msg);
		return ftp_get_msg(self);
	}
	multi_line = 0;
	msg[3] = 0;
	self->r.code    = atoi(msg);
	self->r.reply   = msg;
	self->r.message = msg+4;
	printout(vDEBUG, "[%d] '%s'\n", self->r.code, self->r.message);

	/* check errors that may occur to every process and return a specific error number */
	
	/* TODO USS shall these return an error-level or simply be discarded? 
	 * TODO USS at least FTP_ERROR(x) does not see ERR_POSITIVE_PRELIMARY as an error*/
	if(self->r.reply[0] == '1')
		return ERR_POSITIVE_PRELIMARY;
		
	if(self->r.reply[0] == '2' || self->r.reply[0] == '3') 
		return 0;
		
	/* rfc says that on 4xx errors, the command can be retried as it was */
	if(self->r.reply[0] == '4')
		return ERR_RETRY;
		
	if(self->r.reply[0] == '5')
		return ERR_PERMANENT;

	/* when this is reached there must be something wrong */
	printout(vLESS, "Dead code reached by FTP-reply:\n%d %s\n", 
	         self->r.code, self->r.message);
	return 0;
}

/* puts the string in self->sbuf into the socket. */
void ftp_send_msg(ftp_con * self) {
	if(strncmp(self->sbuf, "PASS", 4) != 0)
		printout(vDEBUG, "---->%s", self->sbuf);
	socket_write(self->sock, self->sbuf, strlen(self->sbuf));
}

/* sends a command to the server. */
void ftp_issue_cmd(ftp_con * self, char * cmd, char * value) {
	int len = strlen(cmd)
		+ ((value) ? strlen(value) + 1 : 0) /* value + space */ 
		+ 2  /* \r\n */ 
		+ 1; /* \0 */

	/* a new buffer if the old one is not big enough anymore */
	if(self->sbuflen < len) {
		self->sbuf    = realloc(self->sbuf, len);
		self->sbuflen = len; 
	}
	memset(self->sbuf, 0, self->sbuflen);
	
	/* i don't trust sprintf */
	strcpy(self->sbuf, cmd);
	if(value) {
		int pos = strlen(cmd);
		self->sbuf[pos] = ' ';
		strcpy(self->sbuf+pos+1, value);
	}
	strncpy(self->sbuf+strlen(self->sbuf), "\r\n\0", 3);
	
	ftp_send_msg(self);
}

/* =================================== *
 * ========== api-routines =========== *
 * =================================== */
 
/* establish the connection for the control-socket */
/* error-levels: ERR_FAILED */
int ftp_connect(ftp_con * self, proxy_settings * ps) {
	int res = 0;
	/* if we have a previous connection, close it before
	* creating a new one */
	printout(vNORMAL, _("Connecting to %s:%d... "), 
		self->host->ip ? printip((unsigned char *) &self->host->ip) : self->host->hostname,
		self->host->port);
	
	if(ps->type != PROXY_OFF)
		self->sock = proxy_connect(ps, self->host->ip, self->host->port, self->host->hostname);
	else
		self->sock = socket_connect(self->host->ip, self->host->port);
	
	if(!self->sock) {
		printout(vNORMAL, _("failed!\n"));
		return ERR_FAILED;
	}
	printout(vNORMAL, _("connected"));
	
	/* receive the first message of the ftp-server.
	 * this should be done here, because otherwise the login-process has to do it
	 * and if it fails, people think login failed */
	/* ignore 1xy message at the start-point */
	do
		res = ftp_get_msg(self);
	while(res == ERR_POSITIVE_PRELIMARY);
	if(SOCK_ERROR(res))
		return ERR_FAILED;
	if(self->r.code != 220) {
		printout(vNORMAL, _("Connection failed (%s)\n"), self->r.message);
		return ERR_FAILED;
	}
	
	printout(vNORMAL, "! ");
	
	/* We always need to log in and CWD on a new connection */
	self->loggedin     = 0;
	self->needcwd      = 1;
	self->OS           = ST_UNDEFINED;
	self->current_type = TYPE_UNDEFINED;
	self->ps           = ps; /* proxy_settings for data-connections */
	
#ifdef HAVE_SSL
#ifdef WIN32
	if(!ssllib_in_use)
		res = ERR_FAILED;
	else
#endif
	res = ftp_auth_tls(self);
	if(res != 0 && self->secure) {
		printout(vLESS, _("TLS encryption is explicitly required, but could not be established.\n"));
		return ERR_FAILED;
	}
#endif
	printout(vNORMAL, "\n");
	return 0;
}

/* performs the initial login */
/* error-levels: ERR_FAILED, get_msg() */
int ftp_login(ftp_con * self, char * user, char * pass){
	int res = 0;
	/*printout(vDEBUG, "i am %slogged in as %s:%s and want to be %s:%s\n", 
		(self->loggedin ? "" : "not"), self->pass, self->user, user, pass);*/
	if(self->loggedin && SAVE_STRCMP(user, self->user) && SAVE_STRCMP(pass, self->pass))
		return 0;
		
	printout(vNORMAL, _("Logging in as %s ... "), user);
	
	ftp_issue_cmd(self, "USER", user);
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res)) return res;
	
	/* rfc states: 331 need password
	 *             332 need account TODO NRV (do we need support for this?)
	 *             230 logged in */
	if(self->r.code >= 300) {
	/* if we have no password (e.g. ftp://guest@host) the PASS command is
	 * left out unless the server explicitly requires it.
	 * if we have a "" one (e.g. ftp://guest:@host) we'll send it */

		if(self->r.code == 331) {
			if(!pass) {
				printout(vMORE, _("Warning: "));
				printout(vMORE, _("remote server requires a password, but none set. Using an empty one.\n"));
			}
			ftp_issue_cmd(self, "PASS", pass ? pass : "");
			res = ftp_get_msg(self);
			if(SOCK_ERROR(res)) return res;
		
		}
		if(self->r.code == 332) {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("Server requires account login, which is not supported.\n"));
			return ERR_FAILED;
		}
	}
	if(self->r.code != 230) {
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Login-Sequence failed (%s)\n"), self->r.message);
		return ERR_FAILED;
	}
	
	printout(vNORMAL, _("Logged in!\n"));
	self->loggedin = 1;
	self->user     = user ? cpy(user) : NULL;
	self->pass     = pass ? cpy(pass) : NULL;
	return 0;
}

#ifdef HAVE_SSL
/* establish a tls encrypted connection */
/* error-levels: ERR_FAILED, get_msg() */
int ftp_auth_tls(ftp_con * self) {
	int res;
	printout(vMORE, "\n==> AUTH TLS ... ");
	ftp_issue_cmd(self, "AUTH TLS", 0);
	res = ftp_get_msg(self);
	if(self->r.code == 234)
		res = socket_transform_to_ssl(self->sock);
	if(res < 0) printout(vMORE, _("failed (%s).\n"), self->r.message);
	else        printout(vNORMAL, _("encrypted!"));
	return res;
}
/* set some options that are required for tls-encrypted data-connections */
/* error-levels: get_msg() */
int ftp_set_protection_level(ftp_con * self) {
	int res;
	printout(vMORE, _("Setting data protection level to private ... "));
	ftp_issue_cmd(self, "PBSZ 0", 0);
	res = ftp_get_msg(self);
	if(self->r.code != 200) {
		printout(vMORE, "PBSZ failed.\n");
		return res;
	}
	
	/* this is to set the data protection to private. 
	 * [C]lear and some others are also available, but [P]rivate is enough... */
	ftp_issue_cmd(self, "PROT P", 0);
	res = ftp_get_msg(self);
	if(self->r.code != 200) {
		printout(vMORE, "PROT P failed.\n");
		return res;
	}
	printout(vMORE, _("done.\n"));
	return 0;
}
#endif

/* tries to find out which system is running remotely.
 * mainly of interest for parsing the LIST reply */
int ftp_do_syst(ftp_con * self) {
	int res;

	if(self->OS != ST_UNDEFINED) return 0;
	
	printout(vMORE, "==> SYST ... ");
	ftp_issue_cmd(self, "SYST", 0);
	res = ftp_get_msg(self);

	if(res == ERR_PERMANENT) {
		self->OS = ST_OTHER;
		printout(vMORE, _("failed.\n"));
		return 0;
	} else if(res < 0) return res;

	self->OS = ST_OTHER;
	if( self->r.code == 215 ) {
		if (!strncasecmp (self->r.message, "VMS", 3))
			self->OS = ST_VMS;
		else if (!strncasecmp (self->r.message, "UNIX", 4))
			self->OS = ST_UNIX;
		else if (!strncasecmp (self->r.message, "WINDOWS_NT", 10))
			self->OS = ST_WINNT;
		else if (!strncasecmp (self->r.message, "MACOS", 5))
			self->OS = ST_MACOS;
		else if (!strncasecmp (self->r.message, "OS/400", 6))
			self->OS = ST_OS400;
	}
	printout(vMORE, _("done (%s).\n"), self->r.message);
	printout(vDEBUG, "Operating System (enum): %d\n", self->OS);
	return 0;
}

/* send an ABOR command and clear the data-socket */
int ftp_do_abor(ftp_con * self) {
	int res;
	printout(vMORE, "==> ABOR ... ");
	ftp_issue_cmd(self, "ABOR", 0);
	res = ftp_get_msg(self);
	/* if we have to do an abor, we usually have a failed connection, 
	* so the control connection is certainly failed as well... */
	if(SOCK_ERROR(res)) {
		printout(vMORE, _("failed.\n"));
		return ERR_RECONNECT;
	}
	printout(vMORE, _("done.\n"));
	if(self->r.code == 426) {
		printout(vNORMAL, _("Connection cancelled (%s)\n"), self->r.message);
		res = ftp_get_msg(self);
	}
	if(self->datasock)
		socket_close(self->datasock),
		self->datasock = 0;
	return res;
}

/* send the QUIT command and close the sockets */
void ftp_do_quit(ftp_con * self){
	/* if a connection failed we might write on a closed pipe, so check
	 * whether writing is still possible */
	printout(vDEBUG, "Connection ended. (%x)\n", self->sock);
	if(self->sock && socket_is_data_writeable(self->sock->fd, 1)) {
		ftp_issue_cmd(self, "QUIT", 0);
		/* no further error-checking required. we close the connection anyway.
		* so this would not be even necessary. it's just for the ftp-servers to
		* recognize us leaving */
		ftp_get_msg(self);
	}
	if(self->sock)     socket_close(self->sock);
	if(self->datasock) socket_close(self->datasock);
	self->sock     = NULL;
	self->datasock = NULL;
}
/* issues the mdtm command and reads the modification time of the file. the
 * epoch timestamp is stored in *timestamp */
/* i allways have to search hours until i find the document. save time:
 * http://www.ietf.org/internet-drafts/draft-ietf-ftpext-mlst-16.txt */
/* error-levels: ERR_FAILED, get_msg */
int ftp_get_modification_time(ftp_con * self, char * filename, time_t * timestamp) {
	int res;
	struct fileinfo * finfo = NULL;
	struct fileinfo * dl = ftp_get_current_directory_list(self);
	struct tm ts;
	/* if we already have a directory-listing, we can obtain the modification
	 * time from there, otherwise it is better to use the MDTM command if avail-
	 * able */
	/* TODO NRV this is less efficient if the modification time has to be checked
	 * TODO NRV for a huge amount of files of the same directory */
	if(!dl) {
		printout(vMORE, "==> MDTM %s ... ", filename);
		ftp_issue_cmd(self, "MDTM", filename);
		res = ftp_get_msg(self);
		if(SOCK_ERROR(res)) return res;
		/* if the file does not exist remotely, this is ok for us */
		if(self->r.code == 213) {
			//JJJJMMDDHHMMSS
#define MDTM_AT(x) atoi(self->r.message+x); self->r.message[x] = 0
			ts.tm_sec   = MDTM_AT(12);
			ts.tm_min   = MDTM_AT(10);
			ts.tm_hour  = MDTM_AT( 8);
			ts.tm_mday  = MDTM_AT( 6);
			ts.tm_mon   = MDTM_AT( 4);
			ts.tm_year  = atoi(self->r.message) - 1900;
			ts.tm_wday  = 0;
			ts.tm_yday  = 0;
			ts.tm_isdst = -1;
			ts.tm_mon  -= 1; /* decrement month, to have it zerobased */
			/* TODO USS l10n */
			printout(vMORE, _("done (modified on %d.%d.%d at %d:%d:%d)\n"), ts.tm_mday, 
				ts.tm_mon+1, ts.tm_year+1900, ts.tm_hour, ts.tm_min, ts.tm_sec);
			*timestamp = mktime(&ts);
			return 0;
		}
		printout(vMORE, _("failed.\n"));
		if(self->r.code == 551)
			return ERR_FAILED;
		res = ftp_get_list(self);
		if(SOCK_ERROR(res) || res == ERR_FAILED) return res;
		dl = ftp_get_current_directory_list(self);
	}
	if(dl) finfo = fileinfo_find_file(dl, filename);
	if(!finfo) return ERR_FAILED;
	*timestamp = finfo->tstamp;
	return 0;
}

/* retrieve the filesize. if the SIZE command is not available, retrieve
 * a directory-listing and get the size it from there */
/* error-levels: ERR_FAILED (file not found), ERR_TIMEOUT, ERR_RECONNECT */
int ftp_get_filesize(ftp_con * self, char * filename, off_t * filesize){
	int res;
	struct fileinfo * finfo = NULL;
	struct fileinfo * dl    = ftp_get_current_directory_list(self);

	if(!dl) {
		printout(vMORE, "==> SIZE %s ... ", filename);
		ftp_issue_cmd(self, "SIZE", filename);
		res = ftp_get_msg(self);
		if(SOCK_ERROR(res)) return res;
		
		/* TODO USS there might be other codes for 'file not found' */
		if(self->r.code == 213) {
			printout(vMORE, _("done (%s bytes)\n"), self->r.message);
			*filesize = strtoll(self->r.message, NULL, 10);
			return 0;
		}
		printout(vMORE, _("failed.\n"));
		if(self->r.code == 550)
			return ERR_FAILED;
		
		/* otherwise try LIST method */
		res = ftp_get_list(self);
		if(SOCK_ERROR(res) || res == ERR_FAILED) return res;
		dl = ftp_get_current_directory_list(self);
	}
	if(dl) finfo = fileinfo_find_file(dl, filename);
	if(!finfo) return ERR_FAILED;
	*filesize = finfo->size;
	return 0;
}

/* set the transfer-mode to either ascii or binary */
/* error-levels: get_msg() */
int ftp_set_type(ftp_con * self, int type) {
	/* there are other types such as L 36 or E, but i think noone needs them */
	static char * types[] = {"A", "I"};
	int res;
	
	if(self->current_type == type) return 0;
	
	printout(vMORE, "==> TYPE %s ... ", types[type]);
	ftp_issue_cmd(self, "TYPE", types[type]);
	res = ftp_get_msg(self);
	
	if(self->r.code == 200) {
		printout(vMORE, _("done.\n"));
		self->current_type = type;
		return 0;
	}
	printout(vMORE, _("failed.\n"));
	return res;
}

int ftp_do_cwd(ftp_con * self, char * directory) {
	int res;
	
	printout(vMORE, "==> CWD %s", directory);
	ftp_issue_cmd(self, "CWD", directory);
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res))
		return ERR_RECONNECT;
	
	if(self->r.code != 250) {
		printout(vMORE, _(" failed (%s).\n"), self->r.message);
		return ERR_FAILED;
	}
	printout(vMORE, "\n");
	return 0;
}
int ftp_do_mkd(ftp_con * self, char * directory) {
	int res;
	
	printout(vMORE, "==> MKD %s", directory);
	ftp_issue_cmd(self, "MKD", directory);
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res))
		return ERR_RECONNECT;
	
	if(self->r.code != 257) {
		printout(vMORE, _(" failed (%s).\n"), self->r.message);
		return ERR_FAILED;
	}
	printout(vMORE, "\n");
	return 0;
}
/* ask for directory-listing.
 * error-levels: ERR_RECONNECT, ERR_FAILED */
int ftp_do_list(ftp_con * self) {
	int res;
	/* ascii mode is a good idea, esp for listings */
	res = ftp_set_type(self, TYPE_A);
	if(SOCK_ERROR(res))
		return ERR_RECONNECT;
	
	printout(vNORMAL, "==> LIST ... ");
	ftp_issue_cmd(self, "LIST", NULL);
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res))
		return ERR_RECONNECT;
		
	/* we get a 1xy (positive preliminary reply), so the
	 * data-connection is open, but server still wait for the
	 * connection to close unless it will issue the completion command */
	if(self->r.reply[0] != '1') {
		printout(vNORMAL, _("failed.\n"));
		return ERR_FAILED;
	}
	printout(vNORMAL, _("done.\n"));
	
	if(ftp_complete_data_connection(self) < 0)	{
		if(SOCK_ERROR(ftp_do_abor(self))) return ERR_RECONNECT;
		return ERR_FAILED;
	}
	return 0;
}
/* Global (hence stable) pseudo file pointer for Wget ftp_parse_ls(). */
char *ls_next;

/* retrieve a LIST of the current directory */
/* error-levels: ERR_FAILED, get_msg() */
int ftp_get_list(ftp_con * self) {
	int res;
	int size = 1;
	char * list;
	char rbuf[1024];
	struct fileinfo * listing;

	/* retrieve the remote system if not done yet */
	res = ftp_do_syst(self);
	if(res < 0) return res;
	
	res = ftp_establish_data_connection(self);
	if(res < 0) {
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Cannot initiate data-connection (%s)\n"),
			FTP_ERROR(res) || res == ERR_FAILED ? self->r.message : "");
		return res;
	}
	
	res = ftp_do_list(self);
	if(res < 0) return res;
	
	/* start buffer-size. is being reallocated as soon as data arrives.
	 * ugly but works */
	list = malloc(1);
	list[0] = 0;
	
	/* until the socket is done, add everything to the list-buffer */
	while( (res = socket_read(self->datasock, rbuf, 1024-1)) > 0) {
		rbuf[res] = 0;
		size += res;
		list = realloc(list, size);
		strcat(list, rbuf);
	}
	/* make sure the socket gets closed correctly. */
	/* TODO NRV need to check error-level here too? */
	if(res == ERR_TIMEOUT) {
		res = ftp_do_abor(self);
		if(SOCK_ERROR(res)) return ERR_RECONNECT;
	}
	
	if(self->datasock) {
		socket_close(self->datasock);
		self->datasock = NULL;
	}
  
	/* receive the last message about list-completion.
	 * allow 1xy messages, since they might have been discarded before... */
	do
		res = ftp_get_msg(self);
	while(res == ERR_POSITIVE_PRELIMARY);
	
	if(res == ERR_TIMEOUT) {
		/* maybe the connection hangs. try again */
		res = ftp_do_abor(self);
	}
	if(FTP_ERROR(res)) {
		free(list);
		printout(vLESS, _("Error: "));
		printout(vLESS, _("listing directory failed (%s)\n"), self->r.message);
		return res;
	} else if(res < 0) {
		free(list);
		return res;
	}
	
	printout(vDEBUG, "Directory-Listing:\n%s\n-----\n", list);
	ls_next = list;
	listing = ftp_parse_ls(list, self->OS);
	free(list);
	/* add it to the list of known directories */
	self->directorylist = directory_add_dir(self->current_directory, self->directorylist, listing);
	return 0;
}
/* issue the REST command for resuming a file at a certain
 * position */
/* error-levels: ERR_FAILED, ERR_RECONNECT */
int ftp_do_rest(ftp_con * self, off_t filesize) {
	char tmpbuf[21];
	int  res;
	
	printout(vMORE, "==> REST %d ... ", filesize);
	ftp_issue_cmd(self, "REST", int64toa(filesize, tmpbuf, 10));
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res))
		return ERR_RECONNECT;

	if(self->r.code != 350) {
		printout(vMORE, _("failed.\nServer seems not to support resuming. Restarting at 0\n"));
		return ERR_FAILED;
	}
	printout(vMORE, _("done.\n"));
	return 0;
}
/* issue the STOR command which precedes the actual file-transmission
 * TODO IMP catch more errors that are not rfc-conform (such as 451 resuming not allowed. but from where to know? */
/* error-levels: 1 (disable resuming), ERR_FAILED (=> skip), ERR_RETRY, SOCK_ERRORs */
int ftp_do_stor(ftp_con * self, char * filename/*, off_t filesize*/){
  int res;

  printout(vMORE, "==> STOR %s ... ", filename);
  ftp_issue_cmd(self, "STOR", filename);
  res = ftp_get_msg(self);
  /* usually the ftp-server issues st. like 120 go, go, go! *
   * but this might be optional. so when getting a timeout on
   * this, we just ignore it and try to open the data-connection
   * anyway, hoping for success */
  if(res == ERR_TIMEOUT) {
	printout(vMORE, _("[not done, but should be allright]\n"));
	return 0;
  } else if(SOCK_ERROR(res))
	return res;
  else if(res == ERR_PERMANENT)
  	return ERR_FAILED;
  /* we want to distinguish the errors, because some might be
   * recoverable, others not. */
  /* rfc states: 
        125 Data connection already open; transfer starting.
        150 File status okay; about to open data connection.
        226 Closing data connection.
         => retry, but fail at most opt.retry times
        425 Can't open data connection.
         => retry, try switching port/pasv. max 3failures
        450 Requested file action not taken.
            File unavailable (e.g., file busy).
		 => sleep. retry. probably allow more failures.
        451 Requested action aborted: local error in processing.
		 => could mean resuming not allowed (these damned ftp-servers should
		    issue that message, when the REST command is issued)
        452 Requested action not taken.
            Insufficient storage space in system.
		 => fatal error? think so. skip this file
		sometimes:
		553 restart on STOR not yet supported
   * 1 is ok, 5 is cancel, 2 and 4 is st. like retry */
	if(self->r.reply[0] == '1') {
		printout(vMORE, _("done.\n"));
		return 0;
	}
	if(self->r.code == 451 || self->r.code == 553) {
		printout(vMORE, _("failed (%s). (disabling resuming)\n"), self->r.message);
		return 1;
	}
	if((self->r.code == 450 || self->r.code == 226 || self->r.code == 425)) {
		printout(vMORE, _("failed.\n"), self->r.message);
		if(self->r.code == 425) {
			self->portmode = ~self->portmode;
			printout(vMORE, _("Trying to switch PORT/PASV mode\n"));
		}
		return ERR_RETRY;
	}
	printout(vMORE, _("failed (%d %s). (skipping)\n"), self->r.code, self->r.message);
	return ERR_FAILED;
}
/* =================================== *
 * ========= data-connection ========= *
 * =================================== */
/* try to establish a data-connection.
 * either port or pasv mode based upon commandline preference / default-values
 * fall back if either won't work */
/* error-levels: ERR_FAILED, get_msg() */
int ftp_establish_data_connection(ftp_con * self){
	int res;
#ifdef HAVE_SSL
	self->datatls = 0;
	/* prepare ssl-connection if possible */
	if(self->sock->ssl) {
#ifdef WIN32
		if(!ssllib_in_use)
			res = ERR_FAILED;
		else
#endif
		res = ftp_set_protection_level(self);
		if(SOCK_ERROR(res)) return res;
		if(res < 0 && self->secure) return ERR_FAILED;
		if(res == 0) self->datatls = 1;
	}
#endif
	printout(vDEBUG, "Portmode: %d\n", self->portmode);
	if(!self->portmode){
		res = ftp_do_passive(self);
		if(res == ERR_FAILED){
		/* revert back to port mode if passive mode fails,
		* and adjust our settings, so that we don't need
		* to try passive again */
			res = ftp_do_port(self);
			if(res < 0) return res;
			self->portmode = 1;
		}
	}
	else{
		res = ftp_do_port(self);
		if(res == ERR_FAILED){ /* s.a. */
			res = ftp_do_passive(self);
			if(res < 0) return res;
			self->portmode = 0;
		}
	}
	return res;
}
/* this will accept the incoming connection if portmode is used */
/* error-levels: ERR_FAILED */
int ftp_complete_data_connection(ftp_con * self) {
	if(self->portmode) {
		if(self->ps->type == PROXY_SOCKS && self->ps->bind)
			self->datasock =  proxy_accept(self->servsock);
		else {
			self->datasock = socket_accept(self->servsock);
			socket_close(self->servsock);
		}
		self->servsock = NULL;
		
		if(!self->datasock) return ERR_FAILED;
	}
#ifdef HAVE_SSL
#ifdef WIN32
	if(ssllib_in_use)
#endif
	if(self->datatls)
		return socket_transform_to_ssl(self->datasock);
#endif
	return 0;
}
/* try building a connection in passive mode.
 * => connect to an ip/port that the server issued */
/* error-levels: ERR_RECONNECT, ERR_FAILED */
int ftp_do_passive(ftp_con * self) {
	unsigned short sport = 0;
	unsigned int   sip   = 0;
	int res;
	
	printout(vMORE, "==> PASV ... ");
	ftp_issue_cmd(self, "PASV", NULL);
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res)) return ERR_RECONNECT;
	
	if(self->r.code != 227) {
		printout(vMORE, _("failed.\n"));
		return ERR_FAILED;
	}
	printout(vMORE, _("done.\n"));
	
	/* parse the line and extract ip and port from it */
	parse_passive_string(self->r.message, &sip, &sport);
	printout(vDEBUG, "Remote server data port: %s:%d\n", printip((unsigned char *) &sip), sport);
	
	if(self->ps->type == PROXY_OFF)
		self->datasock = socket_connect(sip, sport);
	else
		self->datasock = proxy_connect(self->ps, sip, sport, NULL);
	
	if(!self->datasock) {
		printout(vMORE, _("connection failed.\n"));
		return ERR_FAILED;
	}
	
	return 0;
}

/* listen locally (or on the proxy) and issue the PORT command */
/* error-levels: ERR_FAILED, SOCK_ERRORs */
int ftp_do_port(ftp_con * self){
	unsigned short int sport = 0;
	unsigned       int sip   = 0;
	int res;
	
	/* if we somehow are in the situation that it could possibly
	 * help listening on the proxy (which is extremely unstable
	 * code, but has once worked...) well try it. you have been warned */
	if(self->ps->type == PROXY_SOCKS && self->ps->bind) {
		printout(vMORE, _("Trying to listen on proxy server... "));
		self->servsock = proxy_listen(self->ps, &sip, &sport);
		if(!self->servsock) {
			printout(vMORE, _("failed. Falling back to listen locally\n"));
			printout(vMORE, _("Warning: "));
			printout(vMORE, _(
					"Unless FXP is enabled remotely, your control-connection "
					"should be from the same IP-address, as your PORT bind-request. "
					"So you should consider PASV-mode or reconnect without a proxy.\n"
					));
			self->ps->bind = 0;
		} else
			printout(vMORE, _("done.\n"));
	} else if(self->ps->type == PROXY_HTTP) {
		/* TODO IMP not only warn, but disable port-mode */
		printout(vNORMAL, _("Warning: "));
		printout(vNORMAL, _("Using port-mode. Unable to use the http-proxy for this connection\n"));
	}
	
	if(!self->servsock)
		if(!(self->servsock = socket_listen(self->bindaddr, &sport)))
			return ERR_FAILED;
	
	printout(vMORE, "==> PORT ... ");
	if(!(self->ps->type == PROXY_SOCKS && self->ps->bind)) {
		printout(vDEBUG, "determing local ip_addr\n");
		res = get_local_ip(self->sock->fd, (char *) &sip);
		/* TODO NRV is this so serious that it allows us to cancel the whole process? */
		if(res == ERR_FAILED) Abort(_("Cannot determine local IP address"));
		printout(vDEBUG, "Local IP: %s\n", printip((unsigned char *) &sip));
	}
	
	ftp_issue_cmd(self, "PORT", get_port_fmt(sip, sport));
	res = ftp_get_msg(self);
	if(SOCK_ERROR(res)) return res;
	
	if(self->r.code != 200) {
		printout(vMORE, _("failed.\n"));
		return ERR_FAILED;
	}
	printout(vMORE, _("done.\n"));
	return 0;
}

/* =================================== *
 * ========= directory-list ========== *
 * =================================== */
 
/* 2003-12-09 SMS.
 * Pseudo file read-line function for Wget ftp_parse_ls().
 * Argument is for compatibility only, and is ignored.
 */
char * read_whole_line( FILE *fp)
{
  static char *ls_line;
  char *nl;
  printout(vDEBUG, "read_whole_line. ls_next: %x\n", ls_next);
  
  ls_line = ls_next;
  nl = strchr( ls_line, '\n');

  if (nl == NULL)
  {
    return NULL;
  }
  else
  {
    *nl = '\0';
    ls_next = nl+ 1;
    return ls_line;
  }
}

/* 2003-12-09 SMS.
 * Pseudo file one-character look-ahead function for Wget ftp_parse_ls().
 */
char nextchr( void)
{
  return *ls_next;
}
#ifdef WIN32
void localtime_r(time_t * t, struct tm * res) {
	struct tm * src = localtime(t);
	memcpy(res, src, sizeof(struct tm));
}
#endif
/* ******************** *
 * routines for managing the directory-listing-linked list *
 * this works together with ftp-ls.c */
directory_list * directory_add_dir(char * current_directory, directory_list * A, struct fileinfo * K) {
    if(A == NULL) {
        A = (directory_list *) malloc(sizeof(directory_list));
        A->list = K;
        A->name = cpy(current_directory);
        A->next = NULL;
    } else
        A->next = directory_add_dir(current_directory, A->next, K);
    return A;
}

void ftp_fileinfo_free(ftp_con * self) {
    directory_list  * K = self->directorylist;
    directory_list  * L;
    struct fileinfo * M;
    struct fileinfo * N;
    while(K != NULL) {
        L = K->next;
        free(K->name);
        M = K->list;
        while(M != NULL) {
            N = M->next;
            if(M->name) free(M->name);
            free(M);
            M = N;
        }
        free(K);
        K = L;
    }
}

struct fileinfo * fileinfo_find_file(struct fileinfo * F, char * name) {
    while(F != NULL) {
        if( !strcmp(F->name, name) ) return F;
        F = F->next;
    }
    return NULL;
}
struct fileinfo * ftp_get_current_directory_list(ftp_con * self) {
    directory_list * K = self->directorylist;
    while(K != NULL) {
        if( !strcmp(K->name, self->current_directory) ) return K->list;
        K = K->next;
    }
    return NULL;
}
/* =================================== *
 * ============== utils ============== *
 * =================================== */
/* the ftp-server takes the four bytes of the ip and the two bytes of the port
 * and puts their decimal values in a comma-seperated string like
 * (192,168,1,1,42,5). this function parses the garbage */
void parse_passive_string(char * msg, unsigned int * ip, unsigned short int * port) {
	char * start = strchr(msg, '(') + 1;
	char * curtok;
	char temp[6];
	int i = 0;
	curtok = strtok(start,   ",");
	do
		temp[i++] = atoi(curtok);
	while( (curtok = strtok(NULL, ",") ));

	/* TODO USS do we have an endian problem here for the ip-adress */
	*ip   = *(unsigned int   *) temp;
	*port = ntohs(*(unsigned short *) (temp+4));
}
