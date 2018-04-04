/* Declarations for netrc.c
   Copyright (C) 1995, 1996, 1997 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef NETRC_H
#define NETRC_H

typedef struct _acc_t
{
  char *host;			/* NULL if this is the default machine
				   entry.  */
  char *acc;
  char *passwd;			/* NULL if there is no password.  */
  struct _acc_t *next;
} acc_t;

acc_t *parse_netrc (const char *);
void free_netrc (acc_t *l);

#endif /* NETRC_H */
