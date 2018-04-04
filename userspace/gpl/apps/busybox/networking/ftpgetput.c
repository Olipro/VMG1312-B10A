/* vi: set sw=4 ts=4: */
/*
 * ftpget
 *
 * Mini implementation of FTP to retrieve a remote file.
 *
 * Copyright (C) 2002 Jeff Angielski, The PTR Group <jeff@theptrgroup.com>
 * Copyright (C) 2002 Glenn McGrath
 *
 * Based on wget.c by Chip Rosenthal Covad Communications
 * <chip@laserlink.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "cms_util.h" // brcm
#include "cms_msg.h"

struct globals {
	const char *user;
	const char *password;
	struct len_and_sockaddr *lsa;
	FILE *control_stream;
	int verbose_flag;
	int do_continue;
	int ftp_upgrade;
	char buf[1]; /* actually [BUFSZ] */
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
enum { BUFSZ = COMMON_BUFSIZE - offsetof(struct globals, buf) };
struct BUG_G_too_big {
	char BUG_G_too_big[sizeof(G) <= COMMON_BUFSIZE ? 1 : -1];
};
#define user           (G.user          )
#define password       (G.password      )
#define lsa            (G.lsa           )
#define control_stream (G.control_stream)
#define verbose_flag   (G.verbose_flag  )
#define do_continue    (G.do_continue   )
#define ftp_upgrade    (G.ftp_upgrade   )
#define buf            (G.buf           )
#define INIT_G() do { } while (0)


// brcm begin
#define CPE_FTP_OPER_STATUS_NORMAL           1
#define CPE_FTP_OPER_STATUS_CONNECT_SUCCESS  2
#define CPE_FTP_OPER_STATUS_CONECT_FAILURE   3
#define CPE_FTP_OPER_STATUS_DOWNLOADING      4
#define CPE_FTP_OPER_STATUS_DOWNLOAD_SUCCESS 5
#define CPE_FTP_OPER_STATUS_DOWNLOAD_FAILURE 6
#define CPE_FTP_OPER_STATUS_SAVING           7
#define CPE_FTP_OPER_STATUS_SAVE_FAILURE     8
#define CPE_FTP_OPER_STATUS_UPGRADE_SUCCESS  9
#define CPE_FTP_OPER_STATUS_UPGRADE_FAIL     10
static char *glbImagePtr = NULL;
static int glbUploadSize = 0;
static char *glbCurPtr = NULL;
static int glbStartTime;
static int glbTotal_size = 0;
static int glbRead_total = 0;

static void *msgHandle=NULL;
// brcm end


static void ftp_die(const char *msg) NORETURN;
static void ftp_die(const char *msg)
{
	char *cp = buf; /* buf holds peer's response */

	/* Guard against garbage from remote server */
	while (*cp >= ' ' && *cp < '\x7f')
		cp++;
	*cp = '\0';
	bb_error_msg_and_die("unexpected server response%s%s: %s",
			(msg ? " to " : ""), (msg ? msg : ""), buf);
}

static int ftpcmd(const char *s1, const char *s2)
{
	unsigned n;

	if (verbose_flag) {
		bb_error_msg("cmd %s %s", s1, s2);
	}

	if (s1) {
		fprintf(control_stream, (s2 ? "%s %s\r\n" : "%s %s\r\n"+3),
						s1, s2);
		fflush(control_stream);
	}

	do {
		strcpy(buf, "EOF");
		if (fgets(buf, BUFSZ - 2, control_stream) == NULL) {
			ftp_die(NULL);
		}
	} while (!isdigit(buf[0]) || buf[3] != ' ');

	buf[3] = '\0';
	n = xatou(buf);
	buf[3] = ' ';
	return n;
}

static void ftp_login(void)
{
	/* Connect to the command socket */
	control_stream = fdopen(xconnect_stream(lsa), "r+");
	if (control_stream == NULL) {
		/* fdopen failed - extremely unlikely */
		bb_perror_nomsg_and_die();
	}

	if (ftpcmd(NULL, NULL) != 220) {
		ftp_die(NULL);
	}

	/*  Login to the server */
	switch (ftpcmd("USER", user)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS", password) != 230) {
			ftp_die("PASS");
		}
		break;
	default:
		ftp_die("USER");
	}

	ftpcmd("TYPE I", NULL);
}

static int xconnect_ftpdata(void)
{
	char *buf_ptr;
	unsigned port_num;

/*
TODO: PASV command will not work for IPv6. RFC2428 describes
IPv6-capable "extended PASV" - EPSV.

"EPSV [protocol]" asks server to bind to and listen on a data port
in specified protocol. Protocol is 1 for IPv4, 2 for IPv6.
If not specified, defaults to "same as used for control connection".
If server understood you, it should answer "229 <some text>(|||port|)"
where "|" are literal pipe chars and "port" is ASCII decimal port#.

There is also an IPv6-capable replacement for PORT (EPRT),
but we don't need that.

NB: PASV may still work for some servers even over IPv6.
For example, vsftp happily answers
"227 Entering Passive Mode (0,0,0,0,n,n)" and proceeds as usual.

TODO2: need to stop ignoring IP address in PASV response.
*/

	if (ftpcmd("PASV", NULL) != 227) {
		ftp_die("PASV");
	}

	/* Response is "NNN garbageN1,N2,N3,N4,P1,P2[)garbage]
	 * Server's IP is N1.N2.N3.N4 (we ignore it)
	 * Server's port for data connection is P1*256+P2 */
	buf_ptr = strrchr(buf, ')');
	if (buf_ptr) *buf_ptr = '\0';

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num = xatoul_range(buf_ptr + 1, 0, 255);

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num += xatoul_range(buf_ptr + 1, 0, 255) * 256;

	set_nport(lsa, htons(port_num));
	return xconnect_stream(lsa);
}

static int pump_data_and_QUIT(int from, int to)
{
	/* copy the file */
	if (bb_copyfd_eof(from, to) == -1) {
		/* error msg is already printed by bb_copyfd_eof */
		return EXIT_FAILURE;
	}

	/* close data connection */
	close(from); /* don't know which one is that, so we close both */
	close(to);

	/* does server confirm that transfer is finished? */
	if (ftpcmd(NULL, NULL) != 226) {
		ftp_die(NULL);
	}
	ftpcmd("QUIT", NULL);

	return EXIT_SUCCESS;
}

// brcm begin
static void ftp_log(int status, int totalSize, int doneSize, int elapseTime)
{
   static FILE *ftpFile = NULL;    // store ftp statistics
   ftpFile = fopen ("/var/ftpStats", "w");
   if (ftpFile == NULL)
   {
      bb_error_msg_and_die("ftp error: failed to open file\n");
   }
  
   fprintf(ftpFile, "operStatus = %d totalSize = %d doneSize = %d elapseTime = %d\n", 
                     status, totalSize/1024, doneSize/1024, elapseTime);
   fclose(ftpFile);
}
static int myWrite(char *inBuf, int inBufLen)
{
	static SINT32 allocSize = 0;
   
	if (glbCurPtr == NULL) 
	{
		if (inBufLen < 512)   // not enough data for a valid first packet and exit
		{
			bb_error_msg("ftpgetput myWrite exit inBufLen=%d\n", inBufLen);
			return -1;   
		}
		// Allocate maximum flash image size + possible broadcom header TAG.
		// (Don't bother getting the length from the broadcom TAG, we don't
		// get a TAG if it is a whole image anyways.)
		// The Linux kernel will not assign physical pages to the buffer
		// until we write to it, so it is OK if we allocate a little more
		// than we really need.
		allocSize = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
		bb_error_msg("Allocating %d bytes for flash image.\n", allocSize);
		if ((glbCurPtr = (char *) malloc(allocSize)) == NULL)
		{
			bb_error_msg("Not enough memory error.  Could not allocate %u bytes.", allocSize);   
			return -1;
		}
		bb_error_msg("Memory allocated\n");
		glbImagePtr = glbCurPtr;
	}

	// copy the data from the current packet into our buffer
	if (glbUploadSize + inBufLen < allocSize)
	{
		memcpy(glbCurPtr, inBuf, inBufLen);
		glbCurPtr += inBufLen;
		glbUploadSize += inBufLen;
	}
	else
	{
		bb_error_msg("Image could not fit into %u byte buffer.\n", allocSize);
		return -1;
	}
	
	return inBufLen;
}

// from copyfd.c function bb_full_fd_action
static size_t brcm_get_ftp_data(int src_fd, const size_t size)
{
	size_t read_total = 0;
	struct timeval tim;
	int ftp_status = CPE_FTP_OPER_STATUS_NORMAL;
	FILE *ftpPid = NULL;
	int elapseTime, currentTime;
	
   RESERVE_CONFIG_BUFFER(buffer,BUFSIZ);
	
	gettimeofday(&tim, NULL);
	glbStartTime = (int) tim.tv_sec;
	
	// init ftp log
	ftp_log(ftp_status, (int)size, 0,  0);
	
	if ((ftpPid = fopen ("/var/ftpPid", "w")) != NULL) 
	{
		fprintf(ftpPid,"%d\n",getpid()); (void)fclose(ftpPid);
	}
	else 
	{
		bb_perror_msg(bb_msg_write_error);
		return read_total;
	}
	
	glbTotal_size = (int) size;
	currentTime = glbStartTime;
	while ((size == 0) || (read_total < size)) 
	{
		size_t read_try;
		ssize_t read_actual;
	
		if ((size == 0) || (size - read_total > BUFSIZ)) 
		{
			read_try = BUFSIZ;
		} 
		else 
		{
			read_try = size - read_total;
		}
		
		read_actual = safe_read(src_fd, buffer, read_try);
		if (read_actual > 0) 
		{
			if (myWrite(buffer, (int)read_actual) != (int) read_actual) 
			{
				bb_perror_msg(bb_msg_write_error);	/* match Read error below */
				break;
			}
		}
		else if (read_actual == 0) 
		{
			if (size) 
			{
			   bb_error_msg("Unable to read all data");
			}
			break;
		} 
		else 
		{
			/* read_actual < 0 */
			bb_perror_msg("Read error");
			break;
		}
	
		read_total += read_actual;
		glbRead_total = (int) read_total;
		gettimeofday(&tim, NULL);
		if (currentTime < (int) tim.tv_sec) 
		{
			currentTime = (int) tim.tv_sec;
			ftp_log(CPE_FTP_OPER_STATUS_DOWNLOADING, glbTotal_size, glbRead_total, (currentTime - glbStartTime));
		}
	}
	
	RELEASE_CONFIG_BUFFER(buffer);
	
	gettimeofday(&tim, NULL);
	elapseTime = (int) tim.tv_sec - glbStartTime;
	printf(" *** received ftp size = %d, need size = %d and %d seconds elapsed\n", read_total, size, elapseTime);
	if (read_total != size) 
	{
		ftp_log(CPE_FTP_OPER_STATUS_DOWNLOAD_FAILURE, (int)size, read_total, elapseTime);
		return(read_total);
	}
	
	return(read_total);
}
// brcm end

#if !ENABLE_FTPGET
int ftp_receive(const char *local_path, char *server_path);
#else
static
int ftp_receive(const char *local_path, char *server_path)
{
	int fd_data;
	int fd_local = -1;
	off_t beg_range = 0;
	off_t filesize = 0;
	CmsRet ret;

	/* connect to the data socket */
	fd_data = xconnect_ftpdata();

	if (ftpcmd("SIZE", server_path) != 213) {
		do_continue = 0;
	}
	else
	{
		// filesize = XATOOFF(buf + 4);
		filesize = strtoul(buf+4, NULL, 0);
	}

	if (LONE_DASH(local_path)) {
		fd_local = STDOUT_FILENO;
		do_continue = 0;
	}

	if (do_continue) {
		struct stat sbuf;
		/* lstat would be wrong here! */
		if (stat(local_path, &sbuf) < 0) {
			bb_perror_msg_and_die("stat");
		}
		if (sbuf.st_size > 0) {
			beg_range = sbuf.st_size;
		} else {
			do_continue = 0;
		}
	}

	if (do_continue) {
		sprintf(buf, "REST %"OFF_FMT"u", beg_range);
		if (ftpcmd(buf, NULL) != 350) {
			do_continue = 0;
		}
		else 
		{
			filesize -= beg_range;
		}
	}

	if (ftpcmd("RETR", server_path) > 150) {
		ftp_die("RETR");
	}



// brcm begin
	if (ftp_upgrade)
	{
		char connIfName[CMS_IFNAME_LENGTH]={0};

		if (filesize == 0) 
		{     // not getting it from SIZE command, try it on RETR buf
			char *ptr = NULL, *ptr2 = NULL;
			ptr = strchr(buf, '(');
			if (ptr) 
			{
				ptr2 = strchr(ptr, ' ');
				if (ptr2)
				{
					*ptr2 = '\0';
				}
				filesize = strtoul(ptr+1, NULL, 0);
			}
			else
				bb_error_msg("No size info in RETR command\n");
		}
	
	
	
		/*
		* There is a big image coming.  tftp is about to malloc a big buffer
		* and start filling it.  Notify smd so it can do killAllApps or
		* something to make memory available on the modem.
		*/
		if ((ret = cmsImg_saveIfNameFromSocket(fd_data, connIfName)) != CMSRET_SUCCESS)
		{
			cmsLog_error("could not get ifName for socket %d, ret=%d", fd_data, ret);
			/*
			* We can still go on even if we cannot get connIfName.  smd is able to
			* handle a blank connIfName.
			*/
		}
	
		cmsImg_sendLoadStartingMsg(msgHandle, connIfName);
	      
	
		/* get the file */
		if (brcm_get_ftp_data(fd_data, filesize) != filesize) 
		{
			exit(EXIT_FAILURE);
		}
	
		/* close it all down */
		close(fd_data);
	
		if (ftpcmd(NULL, NULL) != 226) 
		{
			ftp_die(NULL);
		}
	
	 	ftpcmd("QUIT", NULL);
	
		bb_error_msg("ftp test suceeds\n");
	
		/*
		* cmsImsg_writeImage will determine the image format and write
		* to flash.  If successful, the system will do a sysMipsSoftReset
		* immediately.  So we will not return from this function call.
		* (But on the desktop, this call does return, so we still have to check the
		* return value.)
		*/
		if ((ret = cmsImg_writeImage(glbImagePtr, glbUploadSize, msgHandle)) != CMSRET_SUCCESS)
		{
			bb_error_msg("Tftp Image failed: Illegal image.\n");
		}
	
		cmsImg_sendLoadDoneMsg(msgHandle);
	
		cmsMsg_cleanup(&msgHandle);
		return(EXIT_SUCCESS);
	}else{
// brcm end
		/* create local file _after_ we know that remote file exists */
		if (fd_local == -1) {
			fd_local = xopen(local_path,
				do_continue ? (O_APPEND | O_WRONLY)
				            : (O_CREAT | O_TRUNC | O_WRONLY)
			);
		}
		cmsMsg_cleanup(&msgHandle); // brcm
		return pump_data_and_QUIT(fd_data, fd_local);
	}
}
#endif

#if !ENABLE_FTPPUT
int ftp_send(const char *server_path, char *local_path);
#else
static
int ftp_send(const char *server_path, char *local_path)
{
	int fd_data;
	int fd_local;
	int response;

	/* connect to the data socket */
	fd_data = xconnect_ftpdata();

	/* get the local file */
	fd_local = STDIN_FILENO;
	if (NOT_LONE_DASH(local_path))
		fd_local = xopen(local_path, O_RDONLY);

	response = ftpcmd("STOR", server_path);
	switch (response) {
	case 125:
	case 150:
		break;
	default:
		ftp_die("STOR");
	}

	return pump_data_and_QUIT(fd_local, fd_data);
}
#endif

#if ENABLE_FEATURE_FTPGETPUT_LONG_OPTIONS
static const char ftpgetput_longopts[] ALIGN1 =
	"continue\0" Required_argument "c"
	"verbose\0"  No_argument       "v"
	"username\0" Required_argument "u"
	"password\0" Required_argument "p"
	"port\0"     Required_argument "P"
	;
#endif

int ftpgetput_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ftpgetput_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	const char *port = "ftp";
	CmsLogLevel logLevel = DEFAULT_LOG_LEVEL; // brcm
	CmsRet ret;
	/* socket to ftp server */

#if ENABLE_FTPPUT && !ENABLE_FTPGET
# define ftp_action ftp_send
#elif ENABLE_FTPGET && !ENABLE_FTPPUT
# define ftp_action ftp_receive
#else
	int (*ftp_action)(const char *, char *) = ftp_send;

	/* Check to see if the command is ftpget or ftput */
	if (applet_name[3] == 'g') {
		ftp_action = ftp_receive;
	}
#endif

	INIT_G();
	/* Set default values */
	user = "anonymous";
	password = "busybox@";

// brcm begin
	cmsLog_init(EID_FTP);
	cmsLog_setLevel(logLevel);

	if ((ret = cmsMsg_init(EID_FTP, &msgHandle)) != CMSRET_SUCCESS)
	{
		printf("failed to open comm link with smd, tftp failed.");
		return 0;
	}
// brcm end

	/*
	 * Decipher the command line
	 */
#if ENABLE_FEATURE_FTPGETPUT_LONG_OPTIONS
	applet_long_options = ftpgetput_longopts;
#endif
	opt_complementary = "-2:vv:cc:ff"; /* must have 2 to 3 params; -v and -c count */
	opt = getopt32(argv, "cvfu:p:P:", &user, &password, &port,
					&verbose_flag, &do_continue, &ftp_upgrade);
	argv += optind;

	/* We want to do exactly _one_ DNS lookup, since some
	 * sites (i.e. ftp.us.debian.org) use round-robin DNS
	 * and we want to connect to only one IP... */
	lsa = xhost2sockaddr(argv[0], bb_lookup_port(port, "tcp", 21));
	if (verbose_flag) {
		printf("Connecting to %s (%s)\n", argv[0],
			xmalloc_sockaddr2dotted(&lsa->u.sa));
	}

	ftp_login();
	return ftp_action(argv[1], argv[2] ? argv[2] : argv[1]);
}
