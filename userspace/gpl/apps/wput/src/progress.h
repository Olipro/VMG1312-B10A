#ifndef __PROGRESS_H
#define __PROGRESS_H

#include "wput.h"

#ifndef WIN32

#include <sys/ioctl.h>
#include <unistd.h>
int get_term_width(void);

#endif

/* TIMER */
struct wput_timer;
struct wput_timer * wtimer_alloc();
void wtimer_reset (struct wput_timer *wt);
double wtimer_elapsed (struct wput_timer *wt);

char * time_str(void);


/* PROGRESS_BAR */

void bar_create(_fsession * fsession);
void bar_update(_fsession * fsession, off_t transfered, int transfered_last, struct wput_timer * last);
char * calculate_transfer_rate(double time_diff, off_t tbytes, unsigned char sp);

#endif
