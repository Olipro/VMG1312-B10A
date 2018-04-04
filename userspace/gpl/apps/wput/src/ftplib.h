#ifndef __FTPLIB_H
#define __FTPLIB_H

#include "socketlib.h"
#define SAVE_STRCMP(a,b) ((!a && !b) || (a && b && !strcmp(a,b)))

typedef struct _ftp_reply {
	unsigned short int code;
	char * reply;
	char * message;
} ftp_reply;

typedef struct _host_type {
	unsigned int ip;
	char * hostname;
	unsigned short port;
} host_t;

enum stype
{
  ST_UNIX,
  ST_VMS,
  ST_WINNT,
  ST_MACOS,
  ST_OS400,
  ST_OTHER,
  ST_UNDEFINED
};

enum ftype
{
  FT_PLAINFILE,
  FT_DIRECTORY,
  FT_SYMLINK,
  FT_UNKNOWN
};

struct fileinfo
{
  enum ftype type;		    /* file type */
  char * name;			    /* file name */
  off_t size;			    /* file size */
  time_t tstamp;			    /* time-stamp */
  int perms;
  char * linkto;
  struct fileinfo *prev;	/* ...and next structure. */
  struct fileinfo *next;	/* ...and next structure. */
};

typedef struct _directory_list {
  char * name;
  struct fileinfo * list;
  struct _directory_list * next;
} directory_list;

typedef struct _ftp_connection {
	host_t      * host;
	char        * user;
	char        * pass;
	wput_socket * sock;
	wput_socket * datasock;
	wput_socket * servsock;
	ftp_reply     r;
	char        * sbuf;
	int           sbuflen;
	proxy_settings  * ps;
	
	directory_list  * directorylist;
    char            * current_directory;
	
	unsigned int  local_ip;
	unsigned int  bindaddr;
	
	unsigned char needcwd     :1;
	unsigned char loggedin    :1;
	unsigned char portmode    :1;
	         char current_type:2; /* -1 (undefined), 0 (ascii), 1 binary */
	unsigned char secure      :1; /* 1 tls required */
#ifdef HAVE_SSL
	unsigned char datatls     :1;
#endif
	
	enum stype OS;
} ftp_con;

/* konstruktor */
host_t  * ftp_new_host(unsigned ip, char * hostname, unsigned short port);
ftp_con * ftp_new(host_t * host, int secure);

/* dekonstruktor */
void ftp_free_host(host_t * host);
void ftp_quit(ftp_con * self);

/* basic send/recv-api */
int  ftp_get_msg(ftp_con * self);
void ftp_issue_cmd(ftp_con * self, char * cmd, char * value);

/* ftp-functions */
int  ftp_connect(ftp_con * self, proxy_settings * ps);
int  ftp_login(ftp_con * self, char * user, char * pass);
#ifdef HAVE_SSL
int  ftp_auth_tls(ftp_con * self);
int  ftp_set_protection_level(ftp_con * self);
#endif
int  ftp_do_syst(ftp_con * self);
int  ftp_do_abor(ftp_con * self);
void ftp_do_quit(ftp_con * self);
int  ftp_do_cwd(ftp_con * self, char * directory);
int  ftp_do_mkd(ftp_con * self, char * directory);

int  ftp_get_modification_time(ftp_con * self, char * filename, time_t * timestamp);
int  ftp_get_filesize(ftp_con * self, char * filename, off_t * filesize);
int  ftp_set_type(ftp_con * self, int type);

int  ftp_do_list(ftp_con * self);
int  ftp_get_list(ftp_con * self);
int  ftp_do_rest(ftp_con * self, off_t filesize);
int  ftp_do_stor(ftp_con * self, char * filename/*, off_t filesize*/);

int  ftp_establish_data_connection(ftp_con * self);
int  ftp_complete_data_connection(ftp_con * self);

int  ftp_do_passive(ftp_con * self);
int  ftp_do_port(ftp_con * self);

directory_list  * directory_add_dir(char * current_directory, directory_list * A, struct fileinfo * K);
struct fileinfo * fileinfo_find_file(struct fileinfo * F, char * name);
struct fileinfo * ftp_find_directory(ftp_con * self);
void              ftp_fileinfo_free(ftp_con * self);
struct fileinfo * ftp_get_current_directory_list(ftp_con * self);

void parse_passive_string(char * msg, unsigned int * ip, unsigned short int * port);

#endif
