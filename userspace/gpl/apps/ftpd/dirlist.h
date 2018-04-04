#ifndef __BFTPD_DIRLIST_H
#define __BFTPD_DIRLIST_H

struct hidegroup {
    int data;
    struct hidegroup *next;
};

//void hidegroups_init();
//void hidegroups_end();
//void bftpd_stat(char *name, FILE *client);
#if 1 /* Jennifer, Use FTP to access USB, USB time show 1980-01-01 when using IE broswer */
void dirlist(char *name, FILE *client, char verbose, short int xfertype);
#else
void dirlist(char *name, FILE *client, char verbose);
#endif

#endif
