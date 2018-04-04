/* Declarations for wput.
   Copyright (C) 2003
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
   
#include "progress.h"
#include "utils.h"
#include "windows.h"

#ifdef HAVE_TERMIO
#include <sys/termio.h>
#include <sys/winsize.h>
#endif

/* This file deals with progressbar-style / -display
 * and time-measurement issues
 * Seems as though a few things are stolen from wget ;)
 */
 
#undef TIMER_WINDOWS
#undef TIMER_GETTIMEOFDAY
#undef TIMER_TIME

/* Depending on the OS and availability of gettimeofday(), one and
   only one of the above constants will be defined.  Virtually all
   modern Unix systems will define TIMER_GETTIMEOFDAY; Windows will
   use TIMER_WINDOWS.  TIMER_TIME is a catch-all method for
   non-Windows systems without gettimeofday.

   #### Perhaps we should also support ftime(), which exists on old
   BSD 4.2-influenced systems?  (It also existed under MS DOS Borland
   C, if memory serves me.)  */

#ifdef WIN32
 #define TIMER_WINDOWS
#else  /* not WINDOWS */
 #define TIMER_GETTIMEOFDAY defined (__stub_gettimeofday) || defined (__stub___gettimeofday)
 #ifndef TIMER_GETTIMEOFDAY
  #define TIMER_TIME 1
 #endif
#endif

#ifdef TIMER_GETTIMEOFDAY
typedef struct timeval wput_sys_time;
#endif

#ifdef TIMER_TIME
typedef time_t wput_sys_time;
#endif

#ifdef TIMER_WINDOWS
typedef __int64 wput_sys_time;
#endif

struct wput_timer {
	wput_sys_time start;
	/* don't know whether we'll need elapsed last... *
	double elapsed_last; */
};

struct wput_timer * wtimer_alloc() {
	return (struct wput_timer *) malloc (sizeof (struct wput_timer));
}
/* thx to wget */
void wtimer_sys_set (wput_sys_time *wst) {
#ifdef TIMER_GETTIMEOFDAY
  gettimeofday (wst, NULL);
#endif

#ifdef TIMER_TIME
  time (wst);
#endif

#ifdef TIMER_WINDOWS
  /* We use GetSystemTime to get the elapsed time.  MSDN warns that
     system clock adjustments can skew the output of GetSystemTime
     when used as a timer and gives preference to GetTickCount and
     high-resolution timers.  But GetTickCount can overflow, and hires
     timers are typically used for profiling, not for regular time
     measurement.  Since we handle clock skew anyway, we just use
     GetSystemTime.  */
  FILETIME ft;
  SYSTEMTIME st;
  GetSystemTime (&st);

  /* As recommended by MSDN, we convert SYSTEMTIME to FILETIME, copy
     FILETIME to ULARGE_INTEGER, and use regular 64-bit integer
     arithmetic on that.  */
  SystemTimeToFileTime (&st, &ft);
  *wst = ((__int64) ft.dwHighDateTime << 32) + ft.dwLowDateTime;
#endif
}

void wtimer_reset (struct wput_timer *wt)
{
  /* Set the start time to the current time. */
  wtimer_sys_set (&wt->start);
}

double wtimer_sys_diff (wput_sys_time *wst1, wput_sys_time *wst2) {
#ifdef TIMER_GETTIMEOFDAY
  return ((double)(wst1->tv_sec - wst2->tv_sec) * 1000
	  + (double)(wst1->tv_usec - wst2->tv_usec) / 1000);
#endif

#ifdef TIMER_TIME
  return 1000 * (*wst1 - *wst2);
#endif

#ifdef TIMER_WINDOWS
  /* VC++ 6 doesn't support direct cast of uint64 to double.  To work
     around this, we subtract, then convert to signed, then finally to
     double. => FREAKS!!! */
  //printout(vDEBUG, "\ncalled wtimer. diff: %x\n", (int)(signed __int64)(*wst1 - *wst2) / 10000);
  return (double)(signed __int64)(*wst1 - *wst2) / 10000;
#endif
}

/* Return the number of milliseconds elapsed since the timer was last
   reset.  It is allowed to call this function more than once to get
   increasingly higher elapsed values. */

double wtimer_elapsed (struct wput_timer *wt)
{
  wput_sys_time now;
  wtimer_sys_set (&now);
  return wtimer_sys_diff (&now, &wt->start);
}

/* Return the assessed granularity of the timer implementation, in
   milliseconds.  This is used by code that tries to substitute a
   better value for timers that have returned zero.  */

double
wtimer_granularity (void)
{
#ifdef TIMER_GETTIMEOFDAY
  /* Granularity of gettimeofday varies wildly between architectures.
     However, it appears that on modern machines it tends to be better
     than 1ms.  Assume 100 usecs.  (Perhaps the configure process
     could actually measure this?)  */
  return 0.1;
#endif

#ifdef TIMER_TIME
  return 1000;
#endif

#ifdef TIMER_WINDOWS
  /* According to MSDN, GetSystemTime returns a broken-down time
     structure the smallest member of which are milliseconds.  */
  return 1;
#endif
}
/* Return pointer to a static char[] buffer in which zero-terminated
 * string-representation of TM (in form hh:mm:ss) is printed. */
/* TODO URG calling localtime here caused a segfault on a SuSE9 machine. but why???
 * TODO URG find out and fix! */
char * time_str (void)
{
  static char output[15];
  time_t secs = time (NULL);
  struct tm *ptm = localtime (&secs);
  sprintf (output, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return output;
}


/* ====================================
 * Progress Bar 
 * ====================================
 */


/* speed calculation is based on the average speed of the last 
 * SPEED_BACKTRACE seconds */
#define SPEED_BACKTRACE 15

struct _bar {
	unsigned short int bytes_per_dot;
	unsigned short int dots_per_line;
	unsigned char spacing;
	unsigned short int dots;
	char * last_rate;
	char * last_eta;
	unsigned long transfered;
	int last_transfered[SPEED_BACKTRACE];
} bar = {
	1024, /* For each 1KB one dot */
	50,   /* 50 dots per line */
	10,   /* Leave a space each 10 dots */
	0};

#ifndef HAVE_IOCTL
	/* win32 starts a new line, if we have filled 80chars
	 * and print a \r, so never let this happen :) */
	unsigned short int terminal_width = 79;
#else
	unsigned short int terminal_width = 80;

int get_term_width(void) {
	struct winsize win;
	char * p;
	int termwidth = 80;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == -1 || !win.ws_col) {
        	if ((p = getenv("COLUMNS")) != NULL)
			termwidth = atoi(p);
    	} else
        	termwidth = win.ws_col;
	return termwidth;
}

#endif

/* sp: save us place / use short units */
/* calculate the transfer rate. returns a string as 3K/s or 3 KiB/s
 * we have an array, that states, how many bytes have been transfered
 * in each of the last SPEED_BACKTRACE seconds.
 * Except for the starting phase, we have always the same time difference */
char * calculate_transfer_rate(double time_diff, off_t tbytes, unsigned char sp) {
	char * units[2][4] = {
        {  "B/s",   "K/s",    "M/s",     "G/s" },
		{ " B/s ", " KiB/s", " MiB/s",  " GiB/s" }
    };
    double trate = (double) WINCONV tbytes / (time_diff / 1000);
    static char		buf[12] = " --.--";
    unsigned char unit;
    
    if(tbytes == 0 || time_diff == 0) return buf;

    if(trate < 1024)
		unit = 0, trate = trate;
	else if(trate < (1024 * 1024))
		unit = 1, trate = trate / 1024;
	else if(trate < (1024 * 1024 * 1024))
		unit = 2, trate = trate / (1024 * 1024);
	else /* i don't really believe this is necessary, but we'll see ;) 
		this is also the main reason for long longs... */
		unit = 3, trate = trate / (1024 * 1024 * 1024);
	
    if(trate < 100) 
		sprintf(buf, "%s%.2f%s", (trate < 10) ? " " : "", trate, units[sp][unit]);
	else if(trate < 1000)
		sprintf(buf, "%.1f%s", trate, units[sp][unit]);
	else
		sprintf(buf, " %d%s", (int) trate, units[sp][unit]);
    return buf;
}
/* wrapper for our progress_bar */
char * get_transfer_rate(_fsession * fsession, unsigned char sp) {

	off_t tbytes  = 0;
	int	time_diff = SPEED_BACKTRACE;
	int	i;
	
	for(i=0 ; i<SPEED_BACKTRACE ; i++) {
		if(bar.last_transfered[i] == -1) time_diff--;
		else tbytes += bar.last_transfered[i];
	}
    //printout(vDEBUG, "%d Seconds for %d Bytes\n", time_diff, tbytes);
    return calculate_transfer_rate(time_diff*1000, tbytes, sp);
}

char * calculate_eta(_fsession * fsession, off_t transfered) {
	static char buf[11]   = {0};
	int         time_diff = SPEED_BACKTRACE;
	int         remain;
	off_t       tbytes    = 0;

	int		i;
	for(i=0 ; i<SPEED_BACKTRACE ; i++) {
	       	if(bar.last_transfered[i] == -1)
			time_diff--;
		else
			tbytes += bar.last_transfered[i];
	}
	if(tbytes == 0 || time_diff == 0) return buf;
    
	/* rate = transfered-bytes / time-difference 
	 * eta  = remaining-bytes  / rate */
	remain = (int) (WINCONV (fsession->local_fsize - transfered) * ((double) time_diff * 1000)
	    / (double) WINCONV tbytes / 1000);
    if(remain < 60)
		sprintf(buf, "ETA    %02ds", remain);
	else if(remain < 3600)
		sprintf(buf, "ETA %2d:%02dm", remain / 60, remain % 60);
	else if(remain < 3600 * 24)
		sprintf(buf, "ETA %2d:%02dh", remain / 3600, (remain % 3600) / 60);
	else
		sprintf(buf, "ETA %2d:%02dd", remain / (3600 * 24), (remain % (24 * 3600)) / 3600);
    /* NO, there won't be an eta of weeks or years! 14.4modem times are gone ;). god bless all gprs-users */
	
	return buf;
}

void bar_create(_fsession * fsession)
{
	int i;
	if( opt.verbose < vNORMAL ) return;

#ifdef HAVE_IOCTL
	if(opt.barstyle) {
		terminal_width = get_term_width();
		/* if the terminal is too small some calculations will fail
		* and therefore output rubbish */
		if(terminal_width < 45)
			opt.barstyle = 0;
	}
#endif
	
	for(i=0 ; i<SPEED_BACKTRACE; i++)
		bar.last_transfered[i] = -1;

    if(fsession->local_fname)   /* input-pipe */
        printout(vNORMAL, _("Length: %s"), legible(fsession->local_fsize));

	bar.dots = 0;
	if(fsession->target_fsize > 0) {
		printout(vNORMAL, _(" [%s to go]\n"), legible(fsession->local_fsize - fsession->target_fsize) );
		if(!opt.barstyle) {
			int skipped_k = (int) (fsession->target_fsize/1024);
			int skipped_k_len = numdigit (skipped_k);
			int start_paint = (int) (fsession->target_fsize % (bar.dots_per_line * bar.bytes_per_dot));
	 		if (skipped_k_len < 5)
	    			skipped_k_len = 5;
			printout(vNORMAL, _("%* [ skipped %dK ]\n%* %dK "), skipped_k_len+2, 
				skipped_k,  skipped_k_len - numdigit(skipped_k),
				skipped_k - start_paint / 1024);
			for(;start_paint > 0; start_paint-=bar.bytes_per_dot) {
				putc(',', opt.output);
				if(++bar.dots % bar.spacing == 0)
					putc(' ', opt.output);
			}
		}
	} else {
		printout(vNORMAL, "\n");
		if(!opt.barstyle)
			printout(vNORMAL, "    0K "  );
	}
	fflush(opt.output);
}


/* transfered is the number of transfered bytes + the number of
 *    bytes the remote-file already had (just for determining a percentage...)
 * transfered_last is the number of bytes that were transfered
 *    since last update */
void bar_update(_fsession * fsession, off_t transfered, int transfered_last, struct wput_timer * last) {
	unsigned char percent = (unsigned char) ((double) WINCONV transfered / WINCONV fsession->local_fsize * 100);
	unsigned int time_elapsed;
	static char updatecounter = 1;
	if( opt.verbose < vNORMAL ) return;
	
	time_elapsed = wtimer_elapsed(last);

	/* if at least one second has passed, update the transfer_rate and eta */
	bar.transfered += transfered_last;
	if(updatecounter == 10) {
	        /* printout(vDEBUG, "\nupdate (%l,%d,%d,%d)\n", transfered, transfered_last, (int) wtimer_elapsed(last),bar.transfered); */
        	/* rotate the backtrace buffer */
		/* TODO NRV do not rotate but simple pass the pointer of the cell we update */
        	memcpy(bar.last_transfered, bar.last_transfered+1, sizeof(int)*(SPEED_BACKTRACE-1));
		bar.last_transfered[SPEED_BACKTRACE-1] = bar.transfered;
		bar.transfered = 0;

		updatecounter = 0;

	        /* update the rates */
        	bar.last_rate = get_transfer_rate(fsession, (unsigned char) !opt.barstyle);
		bar.last_eta  = calculate_eta(fsession, transfered);
	}
	/* only update if there is at least 1/10 second gone since the last update */
	if(time_elapsed > 100) {
		updatecounter++;
#ifdef WIN32
		/* add 1/25 second */
		last->start += 1000 * 1000; 
#else
		last->start.tv_usec+=100000;
		if(last->start.tv_usec > 1000000) {
			last->start.tv_usec -= 1000000;
			last->start.tv_sec++;
		}
#endif
		if(opt.barstyle) {
			unsigned short int bar_width = terminal_width - 4 - 2 - 14 - 9 - 15; /* == 36 */
			short int data[2] = {
				(short) ((double) WINCONV fsession->target_fsize / WINCONV fsession->local_fsize  * bar_width), //skipped
				(short) ((double) WINCONV (transfered - fsession->target_fsize) / WINCONV fsession->local_fsize * bar_width), //really transfered
			};
			char * transf = legible(transfered);
			
			/* this line creates the cool bar
			* if we have 100% it will look a litte different
			* (no eta, and we'll display the average speed) */
			if(percent < 100)
				printout(vNORMAL, "\r%s%d%% [%*+%*=>%* ] %s%* %s %s",
					(percent < 10) ? " " : "", percent,
					data[0], data[1],
					bar_width - data[1] - data[0] - 1,
					transf,
					17-strlen(transf),
					bar.last_rate,
					bar.last_eta);
			else
				printout(vNORMAL, "\r%* \r100%%[%*+%*=] %s%* %s", terminal_width,
					data[0], data[1],
					transf, 17-strlen(transf),
					bar.last_rate);
		} else {
			unsigned int dots = transfered_last / bar.bytes_per_dot;
			//printout(vDEBUG, "dots: %d (%d) (%dK)\n", bar.dots, dots, (long int) (transfered / 1024));
			for(;dots > 0;dots--) {
				bar.dots++;
				putc('.', opt.output);
				if(bar.dots % bar.spacing == 0)
					putc(' ', opt.output);
				if(bar.dots == bar.dots_per_line) {
					fprintf(opt.output, "%3ld%% %s\n%5ldK ", 
						(long int) percent,
						bar.last_rate,
						(long int) (transfered / 1024));
					bar.dots = 0;
				}
			}
		}
		fflush(opt.output);
	}
}
