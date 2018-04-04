/* Author: Hagen Fritsch <fritsch+wput-src@in.tum.de>
   (C) 2002-2006 by Hagen Fritsch
    
   This file is part of wput.

   This programm is free software; you can redistribute it and/or
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

/* command-line-parsing routines and core control functions */
   
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#  include <netinet/in.h>
#endif

#include "wput.h"
#include "netrc.h"

#ifdef WIN32
#  include "getopt/getopt.h"
#else
#  define _GNU_SOURCE
#  ifdef HAVE_GETOPT_H
#    include <getopt.h>
#  else
#    ifdef HAVE_GNUGETOPT_GETOPT_H
#      include <gnugetopt/getopt.h>
#    else
#      include "getopt/getopt.h"
#    endif
#  endif
#endif

#ifdef ENABLE_NLS
#  ifdef HAVE_LOCALE_H
#    include <locale.h>
#  endif
#endif

#include "progress.h"
#include "_queue.h"
#include "utils.h"

extern char *optarg;

#ifdef WIN32
const static char * version = "0.6-w32";
#else
const static char * version = "0.6";
#endif

_fsession * fsession_queue_entry_point = NULL;

void commandlineoptions(int argc, char * argv[]);
int  start_fsession();
int  start_ftp();
int  start_recur_ftp();
void read_netrc_file(void);

int main(int argc, char *argv[]){
#ifdef WIN32
	/* i don't know why, but if i call WSAStartup just once, there is an
	 * error when initialising sockets later (10093). quite strange and
	 * took me quite a while to figure out *dang* */
	WSADATA sa;
	if(WSAStartup(MAKEWORD(2,2),&sa) != 0) {
		fprintf(stderr, "1st: Error Initializing Windows Socket DLL (WSAStartup Failed).\nError-Code: 0x%x", GetLastError());
		exit(4);
	}
	if(WSAStartup(MAKEWORD(2,2),&sa) != 0) {
		fprintf(stderr, "2nd: Error Initializing Windows Socket DLL (WSAStartup Failed).\nError-Code: 0x%x", GetLastError());
		exit(4);
	}
#endif

	/* i18n */
#ifdef ENABLE_NLS
	/* LC_MESSAGES is enough as the only thing that is done is
	 * the translation of text-messages */
#  ifdef LC_MESSAGES
	setlocale (LC_MESSAGES, "");
	setlocale (LC_CTYPE, "");
#  else
	setlocale (LC_ALL, "");
#  endif
	/* Set the text message domain.  */
	bindtextdomain ("wput", LOCALEDIR);
	textdomain ("wput");
#endif
 

	/* initialise global options and set default values */
	memset(&opt, 0, sizeof(opt));

	/* create the ssl framework */
#ifdef HAVE_SSL
	SSL_library_init();
#endif

	opt.sbuf      = malloc(82);
	opt.retry     = -1;
	opt.retry_interval = 10;
	opt.time_deviation = 10;
	opt.binary    = TYPE_UNDEFINED;
	opt.output    = stdout;
	
	opt.verbose   = vNORMAL;
	opt.bindaddr  = INADDR_ANY;
	opt.barstyle  = 1;
	opt.ps.bind   = 1;
	opt.session_start = wtimer_alloc();
	
	opt.resume_table.small_large = RESUME_TABLE_UPLOAD;
	opt.resume_table.large_large = RESUME_TABLE_SKIP;
	opt.resume_table.large_small = RESUME_TABLE_RESUME;
	
	opt.email_address = cpy("wput@localhost.com");
	
	/* env overrides home overrides system wputrc */
#ifdef SYSTEM_WPUTRC
	readwputrc(SYSTEM_WPUTRC);
#endif
	readwputrc(NULL); /*homedir*/
	readwputrc(getenv("WPUTRC"));
	
	parse_proxy(getenv("ftp_proxy"));

	read_netrc_file();
	read_password_file(getenv("PASSWORDFILE"));
	
	wtimer_reset(opt.session_start);
	
	commandlineoptions(argc, argv);

#ifndef WIN32
	/* If we are still at stdout, then redirect output to 'wput-log'. */
	if(opt.background) {
		if(opt.output == stdout) {
				printout(vLESS, _("Resuming in background. Logging output to 'wput-log'.\n"));
				opt.output   = fopen("wput-log", "a");
				if(opt.output == NULL) { perror(_("Unable to open logfile")); exit(4); }
				opt.barstyle = 0;
		} else
				puts("Resuming in background.\n");
		
		if(fork() > 0) exit(0); 
		
		if(opt.input_pipe) {
			printout(vNORMAL, _("Warning: "));
			printout(vNORMAL, _("background-mode might not work correctly, if the input-pipe needs to read from stdin (like cat -).\n"));
		}
		if(opt.input != stdin) {
			/* create a new session and release the console-descriptors... */
			setsid ();
			freopen ("/dev/null", "r", stdin);
			freopen ("/dev/null", "w", stdout);
			freopen ("/dev/null", "w", stderr);
		} else {
			printout(vNORMAL, _("Warning: "));
			printout(vNORMAL, _("reading urls from stdin, while running in background-mode is not reliable.\n"));
		}
	}
#endif
	/* this sets the barstyle to the old one unless wput runs on a tty */
	if(opt.barstyle && !isatty( fileno(stdout) ))
		opt.barstyle = 0;
	
	if((opt.ps.ip == 0 || opt.ps.port == 0) && opt.ps.type != PROXY_OFF) {
		printout(vNORMAL, _("Warning: "));
		printout(vNORMAL, _("Ignoring request to turn proxy-usage on, since no proxy is configured.\n"));
		opt.ps.type = PROXY_OFF;
	}
	
	/* process all url/file combinations that were supplied by commandline */
	queue_process(0);
	
	/* read URLs from input-file */
	if(opt.input != 0) read_urls();
	
	/* if there are lonely files remaining, give them the last (user-supplied) url */
	process_missing();
	
	/* now we've everything we need or are already done */
	if(opt.sorturls) {
		printout(vDEBUG, "Transmitting sorted fsessions\n");
		while(fsession_queue_entry_point != NULL) {
				int res = fsession_transmit_file(fsession_queue_entry_point, opt.curftp);
				if(res == -1)      opt.failed++;
				else if(res == -2) opt.skipped++;
				opt.curftp = fsession_queue_entry_point->ftp;
				free_fsession(fsession_queue_entry_point);
		}
	}

	/* finally close any existing connections */
	if(opt.curftp) ftp_quit(opt.curftp);
	
	if(opt.transfered == 0 && opt.skipped == 0 && opt.failed == 0)
		printout(vNORMAL, _("Nothing done. Try `wput --help'.\n"));
	else
		printout(vNORMAL, _("FINISHED --%s--\n"), time_str());
	
	if(opt.transfered > 0)
		printout(vNORMAL, opt.transfered == 1 ?
		_("Transfered %s bytes in %d file at %s\n") : 
		_("Transfered %s bytes in %d files at %s\n"), 
			legible(opt.transfered_bytes),
			opt.transfered,
			calculate_transfer_rate(
				wtimer_elapsed(opt.session_start), 
				opt.transfered_bytes,
				0)
			);
	if(opt.skipped > 0)
		printout(vNORMAL, opt.skipped == 1 ? _("Skipped %d file.\n") : _("Skipped %d files.\n"), opt.skipped);
	if(opt.failed > 0)
		printout(vNORMAL, opt.failed == 1 ? _("Transmission of %d file failed.\n") : _("Transmission of %d files failed.\n"), opt.failed);
	
	/* clean up */
	free(opt.session_start);
	free(opt.email_address);
	free(opt.sbuf);
	
	if(opt.ps.pass)    free(opt.ps.pass);
	if(opt.ps.user)    free(opt.ps.user);
	if(opt.last_url)   free(opt.last_url);
	
	if(opt.pl)         password_list_free(opt.pl);
	skiplist_free(opt.skipdlist);
	
#ifdef MEMDBG
	print_unfree();
#endif
	printout(vDEBUG, "opt.skipped = %d\n", opt.skipped);
	printout(vDEBUG, "opt.failed = %d\n", opt.failed);

	return ((opt.failed != 0) * 2) | (opt.skipped != 0);
}
/* read urls/files from input-file as though they were supplied as command-line-arguments */
int read_urls(void) {
	char * url;
	char * p;
	while ((url = p = read_line(opt.input))) {
		/* skip spaces and skip lines beginning with a # */
		printout(vDEBUG, "read `%s'\n", p);

		while( isspace(*p) ) p++;
		if(*p == '#') {
			free(url);
			continue;
		}
		
		/* just foolproof check for \r\n */
		if(url[strlen(url)-2] == '\r') 
			url[strlen(url)-2] = 0;
		url[strlen(url)-1] = 0;

		if(!strncmp(p, "ftp://", 6))
			queue_add_url(cpy(p));
		else
			queue_add_file(cpy(p));

		free(url);
		/* process anything that's already complete */
		queue_process(0);
	}
	return 0;
}
/* ugly code to parse through the wputrc-options */
int set_option(char * com, char * val) {
  printout(vDEBUG, "Setting option '%s' to '%s'\n", com, val);
  
  switch(*com) {
  case '1':
  case '2':
      if(!strncmp(com, "2_1", 4)) {
          if(!strncasecmp(val, "SKIP", 5))
            opt.resume_table.large_small = RESUME_TABLE_SKIP;
          else 
            opt.resume_table.large_small = !strncasecmp(val, "UPLOAD", 7) ? RESUME_TABLE_UPLOAD : RESUME_TABLE_RESUME;
      } else if(!strncmp(com, "2_2", 4))
            opt.resume_table.large_large = !strncasecmp(val, "UPLOAD", 7) ? RESUME_TABLE_UPLOAD : RESUME_TABLE_SKIP;
      else if(!strncmp(com, "1_2", 4))
            opt.resume_table.small_large = !strncasecmp(val, "UPLOAD", 7) ? RESUME_TABLE_UPLOAD : RESUME_TABLE_SKIP;
      else
            return -1;
      return 0;
  case 'b':
      if(!strncasecmp(com, "bind-address", 13)) {
            if(get_ip_addr(optarg, &opt.bindaddr) == -1) {
				printout(vMORE, _("Error: "));
				printout(vMORE, _("`%s' could not be resolved. "), optarg);
				printout(vLESS, _("Exiting.\n"));
                exit(4);
            }
            return 0;
        } else return -1;
  case 'c':
      if(!strncasecmp(com, "connection_mode", 16)) {
        if(!strncasecmp(val, "pasv", 5)) {
          opt.portmode = 0;
        } else if(!strncasecmp(val, "port", 4)) {
          opt.portmode = 1;
        } else return -1;
      } else return -1;
      return 0;
#ifdef HAVE_SSL
  case 'f':
      if(!strncmp(com, "force_tls", 9))
          opt.tls = !strncasecmp(val, "on", 3);
      else
          return -1;
      return 0;
#endif
  case 'm':
      if(!strncmp(com, "email_address", 13))
          opt.email_address = cpy(val);
      else return -1;
      return 0;
  case 'p':
      if(!strncasecmp(com, "proxy", 6)) {
        if(!strncmp(val, "http", 5))
          opt.ps.type = PROXY_HTTP;
        else if(!strncasecmp(val, "socks", 6)) 
          opt.ps.type = PROXY_SOCKS;
        else
          opt.ps.type = PROXY_OFF;
      }
      else if(!strncasecmp(com, "proxy_host", 11)) {
        if(get_ip_addr(val, &opt.ps.ip) == -1) {
            printout(vLESS, _("Warning: "));
			printout(vLESS, _("`%s' could not be resolved. "), val);
			printout(vLESS, _("Disabling proxy support.\n"));
            opt.ps.type = 0;
        } 
      }
      else if(!strncasecmp(com, "proxy_port", 11))
          opt.ps.port = atoi(val);
      else if(!strncasecmp(com, "proxy_user", 11))
          opt.ps.user = cpy(val);
      else if(!strncasecmp(com, "proxy_pass", 11))
          opt.ps.pass = cpy(val);
      else if(!strncasecmp(com, "proxy_bind", 11))
          opt.ps.bind = !strncasecmp(val, "on", 3);
      else if(!strncasecmp(com, "passwordfile", 13) || !strncasecmp(com, "password_file", 14))
          read_password_file(val);
      else return -1;
      return 0;
    case 'r':
        if(!strncasecmp(com, "rate", 5)) {
          opt.speed_limit = atoi(val);
          while(*val) {
            if(*val == 'K') opt.speed_limit *= 1024;
            if(*val == 'M') opt.speed_limit *= 1024 * 1024;
            val++;
          }
          printout(vDEBUG, "Rate-Limit is set to %d Bytes per second\n", opt.speed_limit);
        } else if(!strncasecmp(com, "retry_count", 12))
            opt.retry = atoi(val);
        else return -1;
        return 0;
  case 's':
      //if(!strncmp(com, "script_file", 12))
        //load_script(val);
      //else
      if(!strncasecmp(com, "sort_urls", 10))
        opt.sorturls = !strncasecmp(val, "on", 3);
      else return -1;
      return 0;
  case 't':
      if(!strncasecmp(com, "timeout", 8))
        socket_set_default_timeout(atoi(val));
      else if(!strncasecmp(com, "timestamping", 13))
        opt.timestamping    = !strncasecmp(val, "on", 3);
      else if(!strncasecmp(com, "timeoffset", 11))
        opt.time_offset     = atoi(val);
      else if(!strncasecmp(com, "timeoffset", 11))
        opt.time_deviation  = atoi(val);
      else if(!strncasecmp(com, "transfer_type", 14)) {
        if(!strncasecmp(val, "auto", 5)) opt.binary = TYPE_UNDEFINED;
        else if(!strncasecmp(val, "ascii", 6))  opt.binary = TYPE_A;
        else if(!strncasecmp(val, "binary", 7)) opt.binary = TYPE_I;
        else return -2;
      } else return -1;
      return 0;
  case 'v':
      if(!strncasecmp(com, "verbosity", 10)) {
        char * levels[] = {"quite", "less", "normal", "more", "debug"};
        int i;
        for(i=0;i<5;i++)
          if(!strcasecmp(val, levels[i])) {
            opt.verbose = i;
            return 0;
          }
        return -2;
      } else return -1;
  case 'w':
      if(!strncasecmp(com, "wait_retry", 11))
        opt.retry_interval = atoi(val);
      else return -1;
      return 0;
  }
  return -1;  
}

#define NETRC_FILE_NAME ".netrc"

/* read the netrc file and store its contents in a linked list */
void read_netrc_file(void) {
    /* Find ~/.netrc.  */
    char *path, *home;
    acc_t *netrc_list, *l;

    home = home_dir ();
    if (!home)
	return;

    path = (char *) malloc(strlen (home) + 1 +
			   strlen (NETRC_FILE_NAME) + 1);
    if (!path)
	return;

    sprintf (path, "%s/%s", home, NETRC_FILE_NAME);
    free (home);

    if (!file_exists(path)) {
	printout(vMORE, _("netrc file '%s' cannot be read. skipping\n"), path);
	return;
    }

    printout(vDEBUG, "Reading netrc file '%s'", path);
    netrc_list = parse_netrc (path);
    free(path);

    /* If nothing to do...  */
    if (!netrc_list)
	return;

    for (l = netrc_list; l; l = l->next) {
	if (!l->host)
	    continue;
	opt.pl = password_list_add(opt.pl, cpy(l->host), cpy(l->acc), cpy(l->passwd));
	printout(vDEBUG, "added %s:%s@%s to the password-list (%x)\n", l->acc, l->passwd, l->host, opt.pl);
    }

    free_netrc(netrc_list);
}

/* read the password file and store its contents in a linked list */
void read_password_file(char * f) {
	FILE * fp;
	char * line;
	char * tmp;
	char * user;
	char * pass;
	if(!file_exists(f)) {
		printout(vMORE, _("password_file '%s' cannot be read. skipping\n"), f);
		return;
	}
	printout(vNORMAL, _("Warning: You are using a wput password file. This is deprecated!\n"
			    "         Please consider switch to the widely used netrc-files.\n"));
	printout(vDEBUG, "Reading password-file '%s'", f);
	fp = fopen(f, "r");
	while( (tmp = line = read_line(fp)) ) {
		while(isspace(*tmp) && *tmp) tmp++;
		strtok(tmp, "\t");
		user = strtok(NULL, "\t");
		pass = strtok(NULL, "\t");
		if(*tmp == 0 || *tmp == '#' || user == NULL || pass == NULL) {
			free(line);
			continue;
		}
		if(pass[strlen(pass)-2] == '\r') pass[strlen(pass)-2] = 0;
		else if(pass[strlen(pass)-1] == '\n') pass[strlen(pass)-1] = 0;
		opt.pl = password_list_add(opt.pl, cpy(tmp), cpy(user), cpy(pass));
		printout(vDEBUG, "added %s:%s@%s to the password-list (%x)\n", user, pass, tmp, opt.pl);
		free(line);
	}
}
/* reads a wputrc file. parses its options and gives error-reports if necessary */
void readwputrc(char * f) {
    FILE * fp;
    char * file;
    char * line;
    int    ln   = 1;

    if(f == NULL) {
        char * home = home_dir();
        file = malloc(strlen(home) + 10); /* home + slash + (.wputrc | wput.ini) + 0-char */
        sprintf(file, "%s/%s", home, WPUTRC_FILENAME);
        free(home);
    } else file = cpy(f);

    printout(vDEBUG, "Reading wputrc-file: %s\n", file);
    
    if(!file_exists(file)) {
        printout(vDEBUG, "wputrc-file '%s' is not readable. skipping.\n", file);
        free(file);
        return;
    }
    
    fp = fopen (file, "r");
    if(!fp) {
      printout(vLESS, _("Fatal error while opening '%s': %s\n"), file, strerror (errno));
      free(file);
      return;
    }
    
  while ((line = read_line (fp))) {
    char * tmp = line;
    char * com;
    char * val;
    /* skip leading spaces */
    while(isspace(*tmp) && *tmp) tmp++;
    
    /* discard comment lines */
    if(*tmp == '#' || *tmp == ';' || *tmp == 0) {
        free(line);
        continue;
    }
    com = tmp;
    
    /* search for space, end or '=' */
    while(!isspace(*tmp) && *tmp && *tmp != '=') tmp++;
    *tmp++= 0;
    
    while((isspace(*tmp) || *tmp == '=') && *tmp) tmp++;
    val = tmp;
    
    /* suppress the new-line-char */
    while(*tmp != 0 && *tmp != '\n') tmp++;
    if(*(tmp-1) == '\r') *(tmp-1) = 0;
    else                 * tmp    = 0;

    /* we mis-use tmp to store the ret_val, and print a message if something was not parse-able */
    tmp = (char *) set_option(com, val);
    if(tmp == (char *) -1) printout(vLESS, _("%s#%d: Option '%s' not recognized\n"), file, ln, com);
    if(tmp == (char *) -2) printout(vLESS, _("%s#%d: Unknow value '%s' for '%s'\n"), file, ln, val, com);
    free(line);
    ln++;
  }
  free(file);
}

/* parses all the commandline-options available. */
void commandlineoptions(int argc, char * argv[]){
    int c;

    /* TODO IMP windows doesn't automatically fill *.txt with the 
     * TODO IMP corresponding filenames. So this must be done by us (*urgs*) */

    int option_index = 0;
    static struct option long_options[] =
      {
		{"append-output", 1, 0, 'a'},    //0
		{"ascii", 0, 0, 'A'},
		{"background", 0, 0, 'b'},
		{"basename", 1, 0, 0},
		{"binary", 0, 0, 'B'},
		{"bind-address", 1, 0, 0},       //5
		{"compile-options", 0, 0, 0},    
		{"debug", 0, 0, 'd'},
		{"dont-continue", 0, 0, 0},
		{"force-tls", 0, 0, 0},
		{"help", 0, 0, 'h'},             //10
		{"input-file", 1, 0, 'i'},      
		{"input-pipe", 1, 0, 'I'},      
		{"less-verbose", 0, 0, 0},      
		{"limit-rate", 1, 0, 'l'},
		{"no-directories", 0, 0, 0},     //15
		{"output-file", 1, 0, 'o'},     
		{"port-mode", 0, 0, 'p'},       
		{"proxy", 1, 0, 'Y'},           
		{"proxy-user", 1, 0, 0},
		{"proxy-pass", 1, 0, 0},         //20
		{"quiet", 0, 0, 'q'},           
		{"random-wait", 0, 0, 0},       
		{"remove-source-files", 0, 0, 'R'},
		{"reupload", 0, 0, 'u'},        
		{"script", 1, 0, 'S'},           //25
		{"skip-existing", 0, 0, 0},     
		{"skip-larger", 0, 0, 0},       
		{"sort", 0, 0, 's'},            
		{"timeoffset", 1, 0, 0},        
		{"timeout", 1, 0, 'T'},          //30
		{"timestamping", 0, 0, 'N'},    
		{"tries", 1, 0, 't'},           
		{"use-proxy", 1, 0, 'Y'},       
		{"verbose", 0, 0, 'v'},
		{"version", 0, 0, 'V'},          //35
		{"wait", 1, 0, 'w'},            
		{"waitretry", 1, 0, 0},         
		{0, 0, 0, 0}                    
      };
    while (1)
    {
        c = getopt_long (argc, argv, "Vhbo:a:dqvn:i:I:t:NT:w:Rl:pABsS:u",
                           long_options, &option_index);
                
        if (c == -1)
                break;
        
        switch (c)
        {
        case 0:
            switch(option_index) {
            case 13:  //less-verbose
                opt.verbose--;                            break;
            case  5: set_option("bind-address", optarg);  break;
            case  8: //dont-continue
			if(opt.resume_table.large_small == RESUME_TABLE_RESUME)
				opt.resume_table.large_small = RESUME_TABLE_UPLOAD;
			break;     
            case 22: //random-wait
				opt.random_wait = 1;                      break;
            case 17: //port-mode
                opt.portmode = 1;                         break;
            case 19: set_option("proxy_user", optarg);    break;
            case 20: set_option("proxy_pass", optarg);    break;
            case  6: //compile-options
                fprintf(opt.output, "wput version: %s\n\n#defined options:\n", version);
#ifdef TIMER_GETTIMEOFDAY 
                fprintf(opt.output, "TIMER_GETTIMEOFDAY\n");
#endif
#ifdef HAVE_SSL
                fprintf(opt.output, "HAVE_SSL\n");
#endif
                fprintf(opt.output, "\nUsing %d-Bytes for off_t\n", sizeof(off_t));
                exit(0);
            case 27: //skip-larger
                opt.resume_table.small_large = RESUME_TABLE_SKIP;   break;
            case 26: //skip-existing
                opt.resume_table.large_small = RESUME_TABLE_SKIP;
                opt.resume_table.large_large = RESUME_TABLE_SKIP;
                opt.resume_table.small_large = RESUME_TABLE_SKIP;   break;
            case 29: //timeoffset
                set_option("timeoffset", optarg);                   break;
#ifdef HAVE_SSL
            case 9: //force-tls
                set_option("force_tls", "on");                      break;
#endif
            case 15: //no-directories
                opt.no_directories = 1;                             break;
            case  3: //basename
                opt.basename = optarg;                              break;
            case 37: //waitretry
                opt.retry_interval = atoi(optarg);                  break;
            default:
                fprintf(stderr, _("Option %s should not appear here :|\n"), long_options[option_index].name);
            }
            break;
        case 'o': 
        case 'a': opt.output = fopen(optarg, (c == 'o') ? "w" : "a");
                  if(opt.output == NULL) { perror(_("Unable to open logfile")); exit(4); }
                  opt.barstyle = 0;
                  break;
#ifndef WIN32
        case 'b': opt.background = 1;           break;
#endif
        case 't': 
                  opt.retry = atoi(optarg) + 1;
                  if(opt.retry <= 0) opt.retry = -1;
                  break;
        case 'T': socket_set_default_timeout(atoi(optarg));
                                                break;
        case 'p': opt.portmode = 1;             break;
        case 'n':
                  if(optarg[0] == 'v')      opt.verbose--;
                  if(optarg[0] == 'd')      opt.no_directories = 1;
                  if(optarg[0] == 'c') 
                    if(opt.resume_table.large_small == RESUME_TABLE_RESUME)
                       opt.resume_table.large_small =  RESUME_TABLE_UPLOAD;
                  break;
        case 'u': opt.resume_table.large_large = RESUME_TABLE_UPLOAD; break;
        case 'N': opt.timestamping = 1;         break;
        case 'v': opt.verbose++;                break;
        case 'q': opt.verbose=0;                break;
        case 'd': opt.verbose=vDEBUG;           break;
        case 'w': opt.wait   =atoi(optarg);     break;
        case 'A': opt.binary = TYPE_A;          break;
        case 'B': opt.binary = TYPE_I;          break;
        case 's': opt.sorturls = 1;             break;
        case 'S': Abort("TODO SCRIPTING\n");    break;
        case 'l': set_option("rate", optarg);   break;
        case 'i': 
                  if(!strncmp(optarg, "-", 2)) {
                    printout(vDEBUG, "Reading URLs from stdin\n");
                    opt.input = stdin;
                  } else {
                    printout(vDEBUG, "Reading URLs from `%s'\n", optarg);
                    opt.input = fopen(optarg, "r");
                    if(opt.input == NULL) {
                        perror(optarg);
                        exit(4);
                    }
                  }
                  break;
        case 'I': printout(vNORMAL, _("Warning: "));
                  printout(vNORMAL, _("You supplied an input-pipe. "
                      "This is only to be used as fallback, if no filename "
                      "can be found from the URL. This might not be the desired "
                      "behavour. TODO\n"));
                  opt.input_pipe = optarg;      break;
        case 'R': opt.unlink = 1;               break;
        case 'Y': set_option("proxy", optarg);  break;
        case 'V':
            fprintf(opt.output, _("wput version: %s\n"), version);
            exit(0);
        case 'h':
        default:
            fprintf(stderr, _("Usage: wput [options] [file]... [url]...\n"
"  url        ftp://[username[:password]@]hostname[:port][/[path/][file]]\n\n"
"Startup:\n"
"  -V, --version         Display the version of wput and exit.\n"
"  -h, --help            Print this help-screen\n"));
#ifndef WIN32
			fprintf(stderr, _(
"  -b, --background      go to background after startup\n"));
#endif
			fprintf(stderr, "\n");

			fprintf(stderr, _(
"Logging and input file:\n"
"  -o,  --output-file=FILE      log messages to FILE\n"
"  -a,  --append-output=FILE    append log messages to FILE\n"
"  -q,  --quiet                 quiet (no output)\n"
"  -v,  --verbose               be verbose\n"
"  -d,  --debug                 debug output\n"
"  -nv, --less-verbose          be less verbose\n"
"  -i,  --input-file=FILE       read the URLs from FILE\n"
"  -s,  --sort                  sorts all input URLs by server-ip and path\n"
"       --basename=PATH         snip PATH off each file when appendig to an URL\n"
"  -I,  --input-pipe=COMMAND    take the output of COMMAND as data-source\n"
/* will execute the command with the url and file as param and use its output as input
   for the uploading file */
"  -R,  --remove-source-files   unlink files upon successful upload\n"
"\n"));
			fprintf(stderr, _(
"Upload:\n"
"       --bind-address=ADDR     bind to ADDR (hostname or IP) on local host\n"
"  -t,  --tries=NUMBER          set retry count to NUMBER (-1 means infinite)\n"
"  -nc, --dont-continue         do not resume partially-uploaded files\n"
"  -u,  --reupload              do not skip already completed files\n"
"       --skip-larger           do not upload files if remote size is larger\n"
"       --skip-existing         do not upload files that exist remotely\n"
"  -N,  --timestamping          don't re-upload files unless newer than remote\n"
"  -T,  --timeout=10th-SECONDS  set various timeouts to 10th-SECONDS\n"
"  -w,  --wait=10th-SECONDS     wait 10th-SECONDS between uploads. (default: 0)\n"
"       --random-wait           wait from 0...2*WAIT secs between uploads.\n"
"       --waitretry=SECONDS     wait SECONDS between retries of an upload\n"
"  -l,  --limit-rate=RATE       limit upload rate to RATE\n"
"  -nd, --no-directories        do not create any directories\n"
"  -Y,  --proxy=http/socks/off  set proxy type or turn off\n"
"       --proxy-user=NAME       set the proxy-username to NAME\n"
"       --proxy-pass=PASS       set the proxy-password to PASS\n"
"\n"));
			fprintf(stderr, _(
"FTP-Options:\n"
"  -p,  --port-mode             no-passive, turn on port mode ftp (def. pasv)\n"
"  -A,  --ascii                 force ASCII  mode-transfer\n"
"  -B,  --binary                force BINARY mode-transfer\n"));

#ifdef HAVE_SSL
			fprintf(stderr, _(
"       --force-tls             force the useage of TLS\n"));
#endif
/*"  -f,  --peace                 force wput not to be aggressive\n"*/
/*"  -S,  --script=FILE      TODO USS load a wput-script\n\n"*/
			fprintf(stderr, _(
"\n"
"See wput(1) for more detailed descriptions of the options.\n"
"Report bugs and suggestions via SourceForge at\n"
"http://sourceforge.net/tracker/?group_id=141519\n"));
            exit(0);
        }
    }
    
    /* TODO NRV check if we got any input urls. otherwise print usage information */
    while(optind < argc) {
        if(!strncmp(argv[optind], "ftp://", 6))
            queue_add_url(cpy(argv[optind]));
        else
            queue_add_file(cpy(argv[optind]));
        optind++;
    }
}
