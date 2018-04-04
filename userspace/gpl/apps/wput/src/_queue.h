#ifndef __QUEUE_H
#define __QUEUE_H

void queue_add_entry(char * file, char * url);
void queue_add_file(char * filename);
void queue_add_url(char * url);

void queue_process(int force);
void process_missing(void);

int queue_add_dir(char * dname, char * url, _fsession * fsession);

//_fsession * fsession_queue_add(_fsession * F, _fsession * Q);
_fsession * fsession_insert(_fsession * F, _fsession * Q);
_fsession * build_fsession(char * file, char * url);

skipd_list * skiplist_add_entry(skipd_list * K, int ip, char * host, unsigned short int port, char * user, char * pass, char * dir);
int skiplist_find_entry(int ip, char * host, unsigned short int port, char * user, char * pass, char * dir);
void skiplist_free(skipd_list * K);
void free_fsession(_fsession * F);
//void fsession_sort(void);

password_list * password_list_add(password_list * K, char * host, char * user, char * pass);
password_list * password_list_find(password_list * K, char * host, char * user);
void password_list_free(password_list * K);

directory_list * add_directory(directory_list * A, struct fileinfo * K);

struct fileinfo * find_directory(_fsession * F);
struct fileinfo * fileinfo_find_file(struct fileinfo * F, char * name);
void fileinfo_free(void);
#endif
