#ifndef __WPUT_H
#define __WPUT_H

#include "config.h"

#if SIZEOF_INT != 4
#  error "sizeof(int) must be 4"
#endif
#if SIZEOF_SHORT != 2
#  error "sizeof(short) must be 2"
#endif

#include <fcntl.h>

#ifndef WIN32
#  include <unistd.h>
#  include <dirent.h>

#  include <sys/uio.h>
#  include <strings.h>
#  include <sys/errno.h>
#  include <pwd.h>

#  define WINCONV

#  define WPUTRC_FILENAME ".wputrc"
#  define SYSTEM_WPUTRC "/etc/wputrc"

#  define win32_replace_dirsep(x) x
#else
#  include "windows.h"
#endif


#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SSL
# ifndef WIN32
#  include <gnutls/openssl.h>
# endif
#endif

#include "ftp.h"
#include "constants.h"

struct file_timestamp{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

typedef struct _skipd_list {
  int					ip;
  char *				host;
  unsigned short int	port;
  char *				user;
  char *				pass;
  char *				dir;
  struct _skipd_list *	next;
} skipd_list;

typedef struct _password_list {
  char * host;
  char * user;
  char * pass;
  struct _password_list * next;
} password_list;

struct global_options {
  char * sbuf;
  int sbuflen;
  unsigned int bindaddr;
  
  proxy_settings ps;
  ftp_con * curftp;
  
  char * email_address; /* used as password when loggin in anonymously */
  
  skipd_list     * skipdlist;
  char * last_url;

  password_list * pl;
  /* default table. can change for each fsession */
  _resume_table resume_table;
  
  /* flags */
  unsigned char random_wait :1;
  unsigned char portmode    :1;
           char binary      :2; /* -1 (autoprobe), 0, 1 */
  unsigned char proxy       :2; 
  unsigned char sorturls    :1;
  unsigned char unlink      :1;
  unsigned char proxy_bind  :1;
  unsigned char timestamping:1;
           char time_offset :6; /* +-24 (2^5 = 32 + signed) */
#ifndef WIN32
  unsigned char background  :1;
#endif
  unsigned char barstyle    :1; /* 0 -> old wget-style, 1 -> new one... */
  //unsigned char done		:1;
  unsigned char verbose     :3;
  unsigned char tls         :1; /* 1 -> force tls */
  unsigned char no_directories:1;

  short time_deviation;
  char * basename;

  FILE * output;
  FILE * input;
  char * input_pipe;

  short int wait;
  short int retry;

  struct wput_timer * session_start;
  off_t  transfered_bytes;
  int    files_transfered;
  
  /* stats */
  unsigned short transfered;
  unsigned short failed;
  unsigned short skipped;

  unsigned short int retry_interval;
  unsigned       int speed_limit;
} opt;

extern _fsession * fsession_queue_entry_point;
extern char * email_address;
#ifdef WIN32
  #define dirsep '\\'
  #define dirsepstr "\\"
#else
  #define dirsep '/'
  #define dirsepstr "/"
#endif

void readwputrc(char * f);
void read_password_file(char * f);
int read_urls(void);
int set_option(char * com, char * val);

#endif
