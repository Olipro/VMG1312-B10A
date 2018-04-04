/* Declarations for wput.
   Copyright (C) 1989-1994, 1996-1999, 2001 
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

/* this file contains all the functions that fit nowhere else.
   in this case these are esp. string-functions */
#include "utils.h"
#include "windows.h"
#ifndef WIN32
#include <arpa/inet.h>
#endif

unsigned char get_filemode(char * filename){
/* TODO USS do it like diff and check whether the file contains binary data */
  char * txtfiles[] = {"txt", "c", "java", "cpp", "sh", "f", "f90", "f77", "f95", "bas", 
      "pro", "csh", "ksh", "conf", "htm", "html", "php", "pl", "cgi", "inf", "js", 
      "asp", "bat", "cfm", "css", "dhtml", "diz", "h", "hpp", "ini", "mak", "nfo",
      "shtml", "shtm", "tcl", "pas"};
  int i, k;
  char * suffix;
  int dotpos = strlen(filename);
  while(filename[--dotpos] != '.');

  suffix = (char *)(filename + (++dotpos));
  k = strlen(suffix);
  for(i = 0; k > 0 && i < 14; i ++)
    if(strncasecmp(suffix, txtfiles[i], k) == 0)
      return TYPE_A;
  return TYPE_I;
}
void Abort(char * msg){
  fprintf(opt.output, "%s", msg);
  exit(1);
}

/* linux does not know anymore about itoa and windows
 * does not support 64bit ints. so take that! :)
 * keep in mind that con_unit sould be <= 10, otherwise
 * you get strange output... (me was too lazy to fix it) */
char * int64toa(off_t num, char * buf, int con_unit){
    off_t tmp=num;
    if(num == 0) {
        buf[0] = '0',
        buf[1] = 0;
        return buf;
    }
    while(tmp > 0) {
        buf++;
        tmp /= con_unit;
    }
    *buf = 0;
    while(num > 0) {
        *--buf = '0' + (char) (num % con_unit);
        num /= con_unit;
    }
    return buf;
}

char * get_port_fmt(int ip, unsigned int port) {
    unsigned char b[6];
    static char buf[6 * 4];
    /* TODO USS have we got an endian problem here for the ip-address */
    *         (int *) b    = ip;
    *(unsigned int *)(b+4) = htons(port);
    sprintf(buf, "%d,%d,%d,%d,%d,%d", b[0], b[1], b[2], b[3], b[4], b[5]);
    return buf;        
}

void printout(unsigned char verbose, const char * fmt, ...){

  va_list argp;
  const char *p;
  off_t i;
  char *s;
  //unsigned char counter=0;
  char fmtbuf[256];
  
  if(opt.verbose >= verbose) {
      //vfprintf(fsession.output, fmt, argp);
      
      va_start(argp, fmt);
    
      for(p = fmt; *p != '\0'; p++)
        {
          if(*p != '%')
        {
          putc(*p, opt.output);
          continue;
        }
//    switch_again:
          switch(*++p)
        {
        case 'c':
          i = va_arg(argp, int);
          putc((int) i, opt.output);
          break;
          
        case 'l':
        case 'd':
            if(*p == 'l')
                i = va_arg(argp, off_t);
            else
                i = va_arg(argp, int);
          s = int64toa(i, fmtbuf, 10);
          fputs(s, opt.output);
          break;
    
        case 's':
          s = va_arg(argp, char *);
          if(s != NULL) 
              fputs(s, opt.output);
          break;
    
        case 'x':
          i = va_arg(argp, int);
          sprintf(fmtbuf, "%x", (int) i);
          //s = itoa(i, fmtbuf, 16);
          fputs(fmtbuf, opt.output);
          break;
          
        case '*':
          i = va_arg(argp, int);
          p++;
          while(i-- > 0)
              putc(*p, opt.output);
          break;
	  
        case '%':
          putc('%', opt.output);
          break;
        }
        }
    
      va_end(argp);
      fflush(opt.output);
  }
}
/* adopted from wget */
int file_exists (const char * filename)
{
#ifdef HAVE_ACCESS
  return access (filename, F_OK) >= 0;
#else
  struct stat buf;
  /* windows is in trouble... */
  if(!filename) return 0;
  return stat(filename, &buf) >= 0;
#endif
}

char * home_dir (void)
{
  char *home = getenv ("HOME");

  if (!home)
    {
#ifndef WIN32
      /* If HOME is not defined, try getting it from the password file. 
	   * adopted from wget */
      struct passwd *pwd = getpwuid (getuid ());
      if (!pwd || !pwd->pw_dir)
        return NULL;
      home = pwd->pw_dir;
#else
      home = "C:\\";
      /* #### Maybe I should grab home_dir from registry, but the best
     that I could get from there is user's Start menu.  It sucks!  */
#endif
    }
  return home ? cpy(home) : NULL;
}

char * read_line (FILE *fp)
{
  int length = 0;
  int bufsize = 82;
  char *line = (char *) malloc (bufsize);

  while (fgets (line + length, bufsize - length, fp))
    {
      length += strlen (line + length);

      if (line[length - 1] == '\n')
      	break;

      /* fgets() guarantees to read the whole line, or to use up the
         space we've given it.  We can double the buffer
         unconditionally.  */
      bufsize <<= 1;
      line = realloc (line, bufsize);
    }
  if (length == 0 || ferror (fp))
    {
      free (line);
      return NULL;
    }
  if (length + 1 < bufsize)
    /* Relieve the memory from our exponential greediness.  We say
       `length + 1' because the terminating \0 is not included in
       LENGTH.  We don't need to zero-terminate the string ourselves,
       though, because fgets() does that.  */
    line = realloc (line, length + 1);
  return line;
}
#ifndef WIN32
#ifndef isspace
int isspace(int c) {
  switch(c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n': return 1;
  };
  return 0;
}
#endif
#endif

char * printip(unsigned char * ip) {
    static char rv[16];
    sprintf(rv, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return rv;
}
int hextoi(char h) {
    if(h >= 'A' && h <= 'F')
        return h - 'A' + 10;
    if(h >= 'a' && h <= 'f')
        return h - 'a' + 10;
    if(h >= '0' && h <= '9')
        return h - '0';
    printout(vMORE, _("parse-error in escaped character: %c is not a hexadecimal character\n"), h);
    /* TODO NRV is an error in a hex-char in url really a cause to exit the whole process? */
    exit(1);
}
/* transform things like user%40host.com to user@host.com */
/* overwrites str and returns str. */
char * unescape(char * str) {
	char * ptr = str;
	char * org = str;
	while(*ptr) {
		if(*ptr == '%') {
			ptr += 2;
			*ptr = (hextoi(*(ptr-1)) << 4) + hextoi(*(ptr));
		}
		*(str++) = *(ptr++);
	}
	*str = *ptr;
	return org;
}

/* wait opt.retry_interval seconds */
void retry_wait(_fsession * fsession) {
	if(fsession->retry > 0) fsession->retry--;
	if( fsession->retry > 0 || fsession->retry == -1) {
		printout(vLESS, _("Waiting %d seconds... "), opt.retry_interval);
		sleep(opt.retry_interval);
	}
}

/* parses getenv("ftp_proxy") by understanding the following formats:
 * for socks: "[user:pass@]host[:port]"
 * for ftp:   "ftp://[user:pass@]host[:port] */
void parse_proxy(char * url) {
	char * p, *host;
	if(!url) return;
	
	if(strncmp(url, "http://", 7) != 0) {
		printout(vMORE, _("Warning: "));
		printout(vMORE, _("No http-header found. Assuming socks-proxy $host:$port for `%s'\n"), url);
		opt.ps.type = PROXY_SOCKS;
		url = cpy(url);
	} else {
		url = cpy(url + 7);
		opt.ps.type = PROXY_HTTP;
	}
	/* username / password */
	p = strchr(url, '@');
	if(p)
		*p++ = 0, host = p;
	else
		host = url;
	
	if(p) {
		p = strchr(url, ':');
		if(p) {
			*p++ = 0;
			set_option("proxy_pass", unescape(p));
		}
		set_option("proxy_user", unescape(url));
	}
	/* host / port */
	p = strchr(host, ':');
	if(!p) {
		opt.ps.port = (opt.ps.type == PROXY_SOCKS) ? 1080 : 3128;
		printout(vMORE, _("Warning: "));
		printout(vMORE, _("No port specified. Assuming default port %d.\n"), opt.ps.port);
	} else {
		set_option("proxy_port", p + 1);
		*p = 0;
	}
	set_option("proxy_host", host);
	free(url);
}


/* Engine for legible;         from wget
 * add thousand separators to numbers printed in strings.
 */
static char *
legible_1 (const char *repr)
{
  static char outbuf[48]; /* TODO NRV adjust this value to a serious one. who cares? */
  int i, i1, mod;
  char *outptr;
  const char *inptr;

  /* Reset the pointers.  */
  outptr = outbuf;
  inptr = repr;

  /* Ignore the sign for the purpose of adding thousand
     separators.  */
  if (*inptr == '-')
    {
      *outptr++ = '-';
      ++inptr;
    }
  /* How many digits before the first separator?  */
  mod = strlen (inptr) % 3;
  /* Insert them.  */
  for (i = 0; i < mod; i++)
    *outptr++ = inptr[i];
  /* Now insert the rest of them, putting separator before every
     third digit.  */
  for (i1 = i, i = 0; inptr[i1]; i++, i1++)
    {
      if (i % 3 == 0 && i1 != 0)
    *outptr++ = ',';
      *outptr++ = inptr[i1];
    }
  /* Zero-terminate the string.  */
  *outptr = '\0';
  return outbuf;
}

/* Legible -- return a static pointer to the legibly printed long.  */
char *
legible (off_t l)
{
  char inbuf[24];
  /* Print the number into the buffer.  */
  int64toa(l, inbuf, 10);
  return legible_1 (inbuf);
}

/* Count the digits in a (signed long) integer.  */
int numdigit (long number)
{
  int cnt = 1;
  if (number < 0)
    {
      number = -number;
      ++cnt;
    }
  while ((number /= 10) > 0)
    ++cnt;
  return cnt;
}

#ifndef MEMDBG
/* return a malloced copy of the string */
char * cpy(char * s) {
    char * t = (char *) malloc(strlen(s)+1);
    strcpy(t, s);
    return t;
}
#endif

/* gets the filename */
char * basename(char * p) {
    char * t = p + strlen(p);
    while(*t != dirsep && t != p) t--;
    return (t == p) ? t : t+1;
}

char * snip_basename(char * file) {
	if(opt.basename && file) {
		if(!strncmp(opt.basename, file, strlen(opt.basename)))
			return file+strlen(opt.basename);
		else if(file[0] == '.' && file[1] == dirsep && !strncmp(opt.basename, file+2, strlen(opt.basename)))
			return file+2+strlen(opt.basename);
	}
	return file;
}
/* took me ages to write this cute function (it's probably the one i like most in wput):
 * transforms things like "../..gaga/welt/.././.fool/../../new" to "../new"
 * (usually i don't expect such input, but who knows. works quite well now) */
void clear_path(char * path) {
	char * src = path;
	char * dst = path;
	char * org = path;
	while(*src) {
		if(src[0] == '/' && src[1] == '.' && ((src[2] == '.' && (src[3] == '/' || src[3] == 0 ))|| src[2] == '/' || src[2] == 0) && dst != org) {
			if(src[2] == '.' && (src[3] == '/' || src[3] == 0)) {
				if((dst-2 == org && !strncmp(dst-2, "..", 2)) || !strncmp(dst-3, "/..", 3)) *dst++ = *src++;
				else {
					while(dst >= org && *--dst != '/') ;
					if(dst < org) dst++;
					src+=3;
				}
			} else if(src[2] == '/' || src[2] == 0)
				src += 2;
		} else {
			if(*src == '/' && dst == org) src++;			
			*dst++ = *src++;
		}
	}
	*dst = 0;
}

/* makes from some/path and some/other/path
 * ../other/path
 * requires src and dst to begin and end with a slash */
char * get_relative_path(char * src, char * dst) {
	char * tmp = dst;
	char * mark_src = src;
	char * mark_dst = dst;
	int counter = 1;
	/* find the point where they differ and put the mark after the last
	* common slash */
	while( *src != 0 && *dst != 0 && *src == *dst) {
		if(*src == '/') {
			mark_src = src+1;
			mark_dst = dst+1;
		}
		src++; dst++;
	}
	/* special case where dst is a complete subpart of src */
	if(*src == '/' && *dst == 0) {
		mark_src = src+1;
		mark_dst = dst;
	}
	/* if all of src matches dst, we return the rest of dst */
	if(*src == 0 && *dst == '/') return cpy(dst+1);
	
	/* now count the remaining slashes and add a ../ to the rel path for each of them */
	tmp = mark_src;
	while(*tmp++ != 0) if(*tmp == '/') counter++;
	tmp = malloc(counter * 3 + strlen(mark_dst) + 1);
	*tmp = 0;
	while(counter-- > 0)
		strcat(tmp, "../");
	strcat(tmp, mark_dst);
	/* cut the trailing slash off */
	if(tmp[strlen(tmp)-1] == '/') tmp[strlen(tmp)-1] = 0;
	return tmp;
}

#ifdef WIN32
char * win32_replace_dirsep(char * p) {
    char * t = p;
    while(*p != 0) {
        if(*p == dirsep) *p = '/';
        p++;
    }
    return t;
}
#endif

#if 0
int invoke_script(char * event, char * url, char * file) {
    char argv[3] = { event, url, file };
    int oldfd[2]; /* stdin, stdout */
    int res;
    
    /* create some backup pipes */
    if(pipe(oldfd) == -1) printout(vLESS, "Error: Pipe creation failed.\n");
        dup2(0, old[0]);
        dup2(1, old[1]);
        
        printout(vDEBUG, "Redirecting input/output to socket. Invoking Scriptfile... ");
        dup2(s, 0);
        close(s);
        dup2(0, 1);
        res = execv(opt.scriptfile, argv);
        
        /* reconstruct socket, stdin/stdout */
        dup2(0, s);
        close(0);
        close(1);
        dup2(old[0], 0);
        dup2(old[1], 1);
        printout(vDEBUG, "finished.\n");
        
        if(res == -1) printout(vLESS, "Invokation of '%s' failed.\n", opt.scriptfile);
}
#endif
