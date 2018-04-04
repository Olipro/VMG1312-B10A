#ifndef __BFTPD_MAIN_H
#define __BFTPD_MAIN_H

#include <sys/types.h>

struct bftpd_childpid {
	pid_t pid;
	int sock;
};

extern int global_argc;
extern char **global_argv;
#if 0 //brcm
extern FILE *passwdfile, *groupfile, *devnull;
#endif //brcm
extern struct sockaddr_in name;
#if 1 //__MSTC__, Nelson, Refer to Sinjia(DT:SPRID120508496) Remote MGMT function for FTP with ipv6 fail
extern struct sockaddr_in6 v6TOv4;
#endif
extern char *remotehostname;
#if 1 //__MSTC__, Nelson, Refer to Sinjia(DT:SPRID120508496) Remote MGMT function for FTP with ipv6 fail
extern struct sockaddr_storage remotename;
#else
extern struct sockaddr_in remotename;
#endif
extern int control_timeout, data_timeout;
extern int alarm_type;

/* Command line options */
char *configpath;
int daemonmode;

void print_file(int number, char *filename);

#endif
