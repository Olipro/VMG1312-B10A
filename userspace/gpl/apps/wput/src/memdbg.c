#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#ifndef WIN32
#include <sys/socket.h>
#include <unistd.h>
#else
#include <io.h>
#include <winsock.h>
//#include <winsock2.h>
#endif
#include <string.h>

int _mal_count = 0;
int _sock_count= 0;
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif

/* this is for debugging malloc(), realloc() and free() calls.
 * esp useful to find memory leaks, false free()-calls etc */
 
typedef struct buffers {
    void * ptr;
    struct buffers * next;
} _buffers;
_buffers * b_entry = NULL;

_buffers * add_list(_buffers * b, void * ptr) {
    if(b == NULL) {
        b = malloc(sizeof(_buffers));
        b->ptr = ptr;
        b->next = NULL;
        return b;
    }
    b->next = add_list(b->next, ptr);
    return b;
}
_buffers * del_list(_buffers * b, void * ptr) {
    if(b == NULL) {
        printf("FATAL ERROR: tried to free a buffer, not malloced\n");
        exit(1);
    }
    if(b->ptr == ptr) {
        void * p = b->next;
        free(b);
        return p;
    }
    b->next = del_list(b->next, ptr);
    return b;
}

void print_unfreed_buffers(_buffers * b) {
    if(b == NULL) return;
    printf("%x\t", b->ptr);
    printf("%s\n", b->ptr);
    print_unfreed_buffers(b->next);
}
void print_unfree(void) {
    print_unfreed_buffers(b_entry);
}
void * dbg_realloc(void * ptr, size_t size, char * file, int line) {
    printf("%s:%d\t:realloc: %x - realloc(%d) - %d buffers registered\n", file, line, ptr, size, _mal_count);
    b_entry = del_list(b_entry, ptr);
    ptr = realloc(ptr, size);
    b_entry = add_list(b_entry, ptr);
    return ptr;
}
void * dbg_malloc(size_t size, char * file, int line) {
    void * ptr = malloc(size);
    printf("%s:%d\t:malloc: %x - malloc(%d) - %d buffers registered\n", file, line, ptr, size, ++_mal_count);
    b_entry = add_list(b_entry, ptr);
    return ptr;
}
void dbg_free(void * ptr, char * file, int line) {
    if(ptr == NULL) {
        printf("%s:%d\t:free: tried to free a (null)-pointer\n", file, line);
        return;
    }
    printf("%s:%d\t:free: %x (%s)\t%d buffers registered\n", file, line, ptr, ptr, --_mal_count);
    fflush(stdout);
    b_entry = del_list(b_entry, ptr);
    free(ptr);
}

int dbg_socket(int domain, int type, int protocol, char * file, int line) {
    printf("%s:%d\t:socket: %d fds allocated\n", file, line, ++_sock_count);
    return socket(domain, type, protocol);
}
int dbg_shutdown(int s, int how, char * file, int line) {
    printf("%s:%d\t:shutdown: %d (%d sockets allocated)\n", file, line, s, _sock_count);
    return shutdown(s, how);
}
int dbg_open(const char *path, int flags, char * file, int line) {
    printf("%s:%d\t:open: %d fds allocated\n", file, line, ++_sock_count);
    return open(path, flags);
}
int dbg_close(int fd, char * file, int line) {
    printf("%s:%d\t:close: closing %d\t%d fds allocated\n", file, line, fd, --_sock_count);
    return close(fd);
}
char * dbg_cpy(char * s, char * file, int line) {
	char * t = (char *) dbg_malloc(strlen(s)+1, file, line);
	strcpy(t, s);
	return t;
}
char * dbg_strcat(char * s, const char * p, char * file, int line) {
    if(!strcmp(s, "/incoming/wput/")) {
        printf("%s:%d\t:strcat(\"%s\", \"%s\");\n", file, line, s, p);
    }
    return strcat(s,p);
}
