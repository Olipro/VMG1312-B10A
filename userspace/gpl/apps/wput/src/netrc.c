/* Read and parse the .netrc file to get hosts, accounts, and passwords.
   Copyright (C) 1996, Free Software Foundation, Inc.

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
/* This file used to be kept in synch with the code in Fetchmail, but
   the latter has diverged since.  */

/* This file has been adopted from wget for use in wput by
   Alexander Pohoyda */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <sys/types.h>
#include <errno.h>

#include "utils.h"
#include "netrc.h"

#ifndef errno
extern int errno;
#endif

/* Maybe add NEWENTRY to the account information list, LIST.  NEWENTRY is
   set to a ready-to-use acc_t, in any event.  */
static void
maybe_add_to_list (acc_t **newentry, acc_t **list)
{
  acc_t *a, *l;
  a = *newentry;
  l = *list;

  /* We need an account name in order to add the entry to the list.  */
  if (a && ! a->acc)
    {
      /* Free any allocated space.  */
      free (a->host);
      free (a->acc);
      free (a->passwd);
    }
  else
    {
      if (a)
	{
	  /* Add the current machine into our list.  */
	  a->next = l;
	  l = a;
	}

      /* Allocate a new acc_t structure.  */
      a = (acc_t *)malloc (sizeof (acc_t));
    }

  /* Zero the structure, so that it is ready to use.  */
  memset (a, 0, sizeof(*a));

  /* Return the new pointers.  */
  *newentry = a;
  *list = l;
  return;
}

/* Helper function for the parser, shifts contents of
   null-terminated string once character to the left.
   Used in processing \ and " constructs in the netrc file */
static void
shift_left(char *string)
{
  char *p;
  
  for (p=string; *p; ++p)
    *p = *(p+1);
}

/* Parse a .netrc file (as described in the ftp(1) manual page).  */
acc_t *
parse_netrc (const char *path)
{
  FILE *fp;
  char *line, *p, *tok, *premature_token;
  acc_t *current, *retval;
  int ln, quote;

  /* The latest token we've seen in the file.  */
  enum
  {
    tok_nothing, tok_account, tok_login, tok_macdef, tok_machine, tok_password
  } last_token = tok_nothing;

  current = retval = NULL;

  fp = fopen (path, "r");
  if (!fp)
    {
      fprintf (stderr, "Cannot read %s (%s).\n",
	       path, strerror (errno));
      return retval;
    }

  /* Initialize the file data.  */
  ln = 0;
  premature_token = NULL;

  /* While there are lines in the file...  */
  while ((line = read_line (fp)))
    {
      ln ++;

      /* Parse the line.  */
      p = line;
      quote = 0;

      /* Skip leading whitespace.  */
      while (*p && isspace (*p))
	p ++;

      /* If the line is empty, then end any macro definition.  */
      if (last_token == tok_macdef && !*p)
	/* End of macro if the line is empty.  */
	last_token = tok_nothing;

      /* If we are defining macros, then skip parsing the line.  */
      while (*p && last_token != tok_macdef)
	{
	  /* Skip any whitespace.  */
	  while (*p && isspace (*p))
	    p ++;

	  /* discard end-of-line comments; also, stop processing if
	     the above `while' merely skipped trailing whitespace.  */
	  if (*p == '#' || !*p)
	    break;

	  /* if the token starts with quotation mark, note this fact,
	     and squash the quotation character */
	  if (*p == '"'){
	    quote = 1;
	    shift_left (p);
	  }

	  tok = p;

	  /* find the end of the token, handling quotes and escapes.  */
	  while (*p && (quote ? *p != '"' : !isspace (*p))){
	    if (*p == '\\')
	      shift_left (p);
	    p ++;
	  }

          /* if field was quoted, squash the trailing quotation mark
             and reset quote flag */
	  if (quote) {
	    shift_left(p);
            quote = 0;
          }

	  /* Null-terminate the token, if it isn't already.  */
	  if (*p)
	    *p ++ = '\0';

	  switch (last_token)
	    {
	    case tok_login:
	      if (current)
		current->acc = strdup (tok);
	      else
		premature_token = "login";
	      break;

	    case tok_machine:
	      /* Start a new machine entry.  */
	      maybe_add_to_list (&current, &retval);
	      current->host = strdup (tok);
	      break;

	    case tok_password:
	      if (current)
		current->passwd = strdup (tok);
	      else
		premature_token = "password";
	      break;

	      /* We handle most of tok_macdef above.  */
	    case tok_macdef:
	      if (!current)
		premature_token = "macdef";
	      break;

	      /* We don't handle the account keyword at all.  */
	    case tok_account:
	      if (!current)
		premature_token = "account";
	      break;

	      /* We handle tok_nothing below this switch.  */
	    case tok_nothing:
	      break;
	    }

	  if (premature_token)
	    {
	      fprintf (stderr, "\
%s:%d: warning: \"%s\" token appears before any machine name\n",
		       path, ln, premature_token);
	      premature_token = NULL;
	    }

	  if (last_token != tok_nothing)
	    /* We got a value, so reset the token state.  */
	    last_token = tok_nothing;
	  else
	    {
	      /* Fetch the next token.  */
	      if (!strcmp (tok, "account"))
		last_token = tok_account;
	      else if (!strcmp (tok, "default"))
		{
		  maybe_add_to_list (&current, &retval);
		}
	      else if (!strcmp (tok, "login"))
		last_token = tok_login;

	      else if (!strcmp (tok, "macdef"))
		last_token = tok_macdef;

	      else if (!strcmp (tok, "machine"))
		last_token = tok_machine;

	      else if (!strcmp (tok, "password"))
		last_token = tok_password;

	      else
		fprintf (stderr, "%s:%d: unknown token \"%s\"\n",
			 path, ln, tok);
	    }
	}

      free (line);
    }

  fclose (fp);

  /* Finalize the last machine entry we found.  */
  maybe_add_to_list (&current, &retval);
  free (current);

  /* Reverse the order of the list so that it appears in file order.  */
  current = retval;
  retval = NULL;
  while (current)
    {
      acc_t *saved_reference;

      /* Change the direction of the pointers.  */
      saved_reference = current->next;
      current->next = retval;

      /* Advance to the next node.  */
      retval = current;
      current = saved_reference;
    }

  return retval;
}


/* Free a netrc list.  */
void
free_netrc(acc_t *l)
{
  acc_t *t;

  while (l)
    {
      t = l->next;
      free (l->acc);
      free (l->passwd);
      free (l->host);
      free (l);
      l = t;
    }
}
