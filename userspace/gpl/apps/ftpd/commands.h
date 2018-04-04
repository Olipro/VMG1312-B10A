#ifndef __BFTPD_COMMANDS_H
#define __BFTPD_COMMANDS_H

#include <config.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
  #include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif

#include "commands_admin.h"

enum {
    STATE_CONNECTED, STATE_USER, STATE_AUTHENTICATED, STATE_RENAME, STATE_ADMIN
};

enum {
    TYPE_ASCII, TYPE_BINARY
};

#define USERLEN 30
#define USERLEN_S "30"
#define MAXCMD 255

extern int state;
extern char user[USERLEN + 1];
#if 1 //__MSTC__, Nelson, Refer to Sinjia(DT:SPRID120508496) Remote MGMT function for FTP with ipv6 fail
extern struct sockaddr_storage sa;
#else
extern struct sockaddr_in sa;
#endif
extern char pasv;
extern int sock;
extern int transferring;
extern int pasvsock;
extern char *philename;
extern int offset;
extern int ratio_send, ratio_recv;
extern int bytes_stored, bytes_recvd;
extern int xfer_bufsize;

void control_printf(char success, char *format, ...);

void init_userinfo();
void new_umask();
int parsecmd(char *);
int dataconn();
void command_user(char *);
void command_pass(char *);
void command_pwd(char *);
void command_type(char *);
void command_port(char *);
void command_stor(char *);
void command_retr(char *);
void command_list(char *);
void command_syst(char *);
void command_cwd(char *);
void command_cdup(char *);
void command_dele(char *);
void command_mkd(char *);
void command_rmd(char *);
void command_noop(char *);
void command_rnfr(char *);
void command_rnto(char *);
void command_rest(char *);
void command_abor(char *);
void command_quit(char *);
void command_help(char *);
void command_stat(char *);
void command_feat(char *);
void command_fwupdate(char *);   //brcm 
#if 1 //__MSTC__, Nick Tseng
void command_configBackup(char *);
#endif

struct command {
  char *name;
#ifndef SUPPORT_FTPD_STORAGE
  char *syntax;
#endif
  void (*function)(char *);
  char state_needed;
  char showinfeat;
};

#endif
