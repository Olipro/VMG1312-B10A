#ifndef __FTP_H
#define __FTP_H
#include "ftplib.h"

/* the first is for the local file, the second for the remote one.
 * small_large means remote is bigger then local */
typedef struct __resume_table {
	#define RESUME_TABLE_SKIP	 0
	#define RESUME_TABLE_UPLOAD  1
	#define RESUME_TABLE_RESUME  2
	unsigned char small_large:1;
	unsigned char large_large:1;
	unsigned char large_small:2;
} _resume_table;

typedef struct ftp_session {
	char    * user;
	char    * pass;
	host_t  * host;
	ftp_con * ftp;
	
	char * local_fname;
	char * target_dname;
	char * target_fname;
	
	off_t local_fsize;
	off_t target_fsize;
	
	time_t local_ftime;
	time_t target_ftime;
	
	short int retry;
	/* flags */
	_resume_table * resume_table;
	
	unsigned char done  :1;
	         char binary :2;
	
	struct fileinfo * directory;
	struct ftp_session * next;
} _fsession;

int do_cwd(_fsession * fsession);
int long_do_cwd(_fsession * fsession);
int try_do_cwd(ftp_con * ftp, char * path, int mkd);

int do_send(_fsession * fsession);

int fsession_transmit_file(_fsession * fsession, ftp_con * ftp);

/* for ftp-ls.c */

struct fileinfo * ftp_parse_ls (const char * listing, const enum stype system_type);

#endif
