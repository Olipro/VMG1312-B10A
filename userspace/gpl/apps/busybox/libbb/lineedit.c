/* vi: set sw=4 ts=4: */
/*
 * Termios command line History and Editing.
 *
 * Copyright (c) 1986-2003 may safely be consumed by a BSD or GPL license.
 * Written by:   Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Used ideas:
 *      Adam Rogoyski    <rogoyski@cs.utexas.edu>
 *      Dave Cinege      <dcinege@psychosis.com>
 *      Jakub Jelinek (c) 1995
 *      Erik Andersen    <andersen@codepoet.org> (Majorly adjusted for busybox)
 *
 * This code is 'as is' with no warranty.
 */

/*
 * Usage and known bugs:
 * Terminal key codes are not extensive, more needs to be added.
 * This version was created on Debian GNU/Linux 2.x.
 * Delete, Backspace, Home, End, and the arrow keys were tested
 * to work in an Xterm and console. Ctrl-A also works as Home.
 * Ctrl-E also works as End.
 *
 * The following readline-like commands are not implemented:
 * ESC-b -- Move back one word
 * ESC-f -- Move forward one word
 * ESC-d -- Delete forward one word
 * CTL-t -- Transpose two characters
 *
 * lineedit does not know that the terminal escape sequences do not
 * take up space on the screen. The redisplay code assumes, unless
 * told otherwise, that each character in the prompt is a printable
 * character that takes up one character position on the screen.
 * You need to tell lineedit that some sequences of characters
 * in the prompt take up no screen space. Compatibly with readline,
 * use the \[ escape to begin a sequence of non-printing characters,
 * and the \] escape to signal the end of such a sequence. Example:
 *
 * PS1='\[\033[01;32m\]\u@\h\[\033[01;34m\] \w \$\[\033[00m\] '
 */
#include "libbb.h"
#include "unicode.h"

#ifdef TEST
# define ENABLE_FEATURE_EDITING 0
# define ENABLE_FEATURE_TAB_COMPLETION 0
# define ENABLE_FEATURE_USERNAME_COMPLETION 0
#endif


/* Entire file (except TESTing part) sits inside this #if */
#if ENABLE_FEATURE_EDITING


#define ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR \
	(ENABLE_FEATURE_USERNAME_COMPLETION || ENABLE_FEATURE_EDITING_FANCY_PROMPT)
#define IF_FEATURE_GETUSERNAME_AND_HOMEDIR(...)
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
#undef IF_FEATURE_GETUSERNAME_AND_HOMEDIR
#define IF_FEATURE_GETUSERNAME_AND_HOMEDIR(...) __VA_ARGS__
#endif


#define SEQ_CLEAR_TILL_END_OF_SCREEN "\033[J"
//#define SEQ_CLEAR_TILL_END_OF_LINE   "\033[K"


#undef CHAR_T
#if ENABLE_UNICODE_SUPPORT
# define BB_NUL ((wchar_t)0)
# define CHAR_T wchar_t
static bool BB_isspace(CHAR_T c) { return ((unsigned)c < 256 && isspace(c)); }
# if ENABLE_FEATURE_EDITING_VI
static bool BB_isalnum(CHAR_T c) { return ((unsigned)c < 256 && isalnum(c)); }
# endif
static bool BB_ispunct(CHAR_T c) { return ((unsigned)c < 256 && ispunct(c)); }
# undef isspace
# undef isalnum
# undef ispunct
# undef isprint
# define isspace isspace_must_not_be_used
# define isalnum isalnum_must_not_be_used
# define ispunct ispunct_must_not_be_used
# define isprint isprint_must_not_be_used
#else
# define BB_NUL '\0'
# define CHAR_T char
# define BB_isspace(c) isspace(c)
# define BB_isalnum(c) isalnum(c)
# define BB_ispunct(c) ispunct(c)
#endif


# if ENABLE_UNICODE_PRESERVE_BROKEN
#  define unicode_mark_raw_byte(wc)   ((wc) | 0x20000000)
#  define unicode_is_raw_byte(wc)     ((wc) & 0x20000000)
# else
#  define unicode_is_raw_byte(wc)     0
# endif


enum {
	/* We use int16_t for positions, need to limit line len */
	MAX_LINELEN = CONFIG_FEATURE_EDITING_MAX_LEN < 0x7ff0
	              ? CONFIG_FEATURE_EDITING_MAX_LEN
	              : 0x7ff0
};

#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
static const char null_str[] ALIGN1 = "";
#endif

/* We try to minimize both static and stack usage. */
struct lineedit_statics {
	line_input_t *state;

	volatile unsigned cmdedit_termw; /* = 80; */ /* actual terminal width */
	sighandler_t previous_SIGWINCH_handler;

	unsigned cmdedit_x;        /* real x (col) terminal position */
	unsigned cmdedit_y;        /* pseudoreal y (row) terminal position */
	unsigned cmdedit_prmt_len; /* length of prompt (without colors etc) */

	unsigned cursor;
	int command_len; /* must be signed */
	/* signed maxsize: we want x in "if (x > S.maxsize)"
	 * to _not_ be promoted to unsigned */
	int maxsize;
	CHAR_T *command_ps;

	const char *cmdedit_prompt;
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
	int num_ok_lines; /* = 1; */
#endif

#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	char *user_buf;
	char *home_pwd_buf; /* = (char*)null_str; */
#endif

#if ENABLE_FEATURE_TAB_COMPLETION
	char **matches;
	unsigned num_matches;
#endif

#if ENABLE_FEATURE_EDITING_VI
#define DELBUFSIZ 128
	CHAR_T *delptr;
	smallint newdelflag;     /* whether delbuf should be reused yet */
	CHAR_T delbuf[DELBUFSIZ];  /* a place to store deleted characters */
#endif
#if ENABLE_FEATURE_EDITING_ASK_TERMINAL
	smallint sent_ESC_br6n;
#endif

	/* Formerly these were big buffers on stack: */
#if ENABLE_FEATURE_TAB_COMPLETION
	char exe_n_cwd_tab_completion__dirbuf[MAX_LINELEN];
	char input_tab__matchBuf[MAX_LINELEN];
	int16_t find_match__int_buf[MAX_LINELEN + 1]; /* need to have 9 bits at least */
	int16_t find_match__pos_buf[MAX_LINELEN + 1];
#endif
};

/* See lineedit_ptr_hack.c */
extern struct lineedit_statics *const lineedit_ptr_to_statics;

#define S (*lineedit_ptr_to_statics)
#define state            (S.state           )
#define cmdedit_termw    (S.cmdedit_termw   )
#define previous_SIGWINCH_handler (S.previous_SIGWINCH_handler)
#define cmdedit_x        (S.cmdedit_x       )
#define cmdedit_y        (S.cmdedit_y       )
#define cmdedit_prmt_len (S.cmdedit_prmt_len)
#define cursor           (S.cursor          )
#define command_len      (S.command_len     )
#define command_ps       (S.command_ps      )
#define cmdedit_prompt   (S.cmdedit_prompt  )
#define num_ok_lines     (S.num_ok_lines    )
#define user_buf         (S.user_buf        )
#define home_pwd_buf     (S.home_pwd_buf    )
#define matches          (S.matches         )
#define num_matches      (S.num_matches     )
#define delptr           (S.delptr          )
#define newdelflag       (S.newdelflag      )
#define delbuf           (S.delbuf          )

#define INIT_S() do { \
	(*(struct lineedit_statics**)&lineedit_ptr_to_statics) = xzalloc(sizeof(S)); \
	barrier(); \
	cmdedit_termw = 80; \
	IF_FEATURE_EDITING_FANCY_PROMPT(num_ok_lines = 1;) \
	IF_FEATURE_GETUSERNAME_AND_HOMEDIR(home_pwd_buf = (char*)null_str;) \
} while (0)
static void deinit_S(void)
{
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
	/* This one is allocated only if FANCY_PROMPT is on
	 * (otherwise it points to verbatim prompt (NOT malloced) */
	free((char*)cmdedit_prompt);
#endif
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	free(user_buf);
	if (home_pwd_buf != null_str)
		free(home_pwd_buf);
#endif
	free(lineedit_ptr_to_statics);
}
#define DEINIT_S() deinit_S()


#if ENABLE_UNICODE_SUPPORT
static size_t load_string(const char *src, int maxsize)
{
	ssize_t len = mbstowcs(command_ps, src, maxsize - 1);
	if (len < 0)
		len = 0;
	command_ps[len] = 0;
	return len;
}
static unsigned save_string(char *dst, unsigned maxsize)
{
# if !ENABLE_UNICODE_PRESERVE_BROKEN
	ssize_t len = wcstombs(dst, command_ps, maxsize - 1);
	if (len < 0)
		len = 0;
	dst[len] = '\0';
	return len;
# else
	unsigned dstpos = 0;
	unsigned srcpos = 0;

	maxsize--;
	while (dstpos < maxsize) {
		wchar_t wc;
		int n = srcpos;
		while ((wc = command_ps[srcpos]) != 0
		    && !unicode_is_raw_byte(wc)
		) {
			srcpos++;
		}
		command_ps[srcpos] = 0;
		n = wcstombs(dst + dstpos, command_ps + n, maxsize - dstpos);
		if (n < 0) /* should not happen */
			break;
		dstpos += n;
		if (wc == 0) /* usually is */
			break;
		/* We do have invalid byte here! */
		command_ps[srcpos] = wc; /* restore it */
		srcpos++;
		if (dstpos == maxsize)
			break;
		dst[dstpos++] = (char) wc;
	}
	dst[dstpos] = '\0';
	return dstpos;
# endif
}
/* I thought just fputwc(c, stdout) would work. But no... */
static void BB_PUTCHAR(wchar_t c)
{
	char buf[MB_CUR_MAX + 1];
	mbstate_t mbst = { 0 };
	ssize_t len;

	len = wcrtomb(buf, c, &mbst);
	if (len > 0) {
		buf[len] = '\0';
		fputs(buf, stdout);
	}
}
# if ENABLE_UNICODE_COMBINING_WCHARS || ENABLE_UNICODE_WIDE_WCHARS
static wchar_t adjust_width_and_validate_wc(unsigned *width_adj, wchar_t wc)
# else
static wchar_t adjust_width_and_validate_wc(wchar_t wc)
#  define adjust_width_and_validate_wc(width_adj, wc) \
	((*(width_adj))++, adjust_width_and_validate_wc(wc))
# endif
{
	int w = 1;

	if (unicode_status == UNICODE_ON) {
		if (wc > CONFIG_LAST_SUPPORTED_WCHAR) {
			/* note: also true for unicode_is_raw_byte(wc) */
			goto subst;
		}
		w = wcwidth(wc);
		if ((ENABLE_UNICODE_COMBINING_WCHARS && w < 0)
		 || (!ENABLE_UNICODE_COMBINING_WCHARS && w <= 0)
		 || (!ENABLE_UNICODE_WIDE_WCHARS && w > 1)
		) {
 subst:
			w = 1;
			wc = CONFIG_SUBST_WCHAR;
		}
	}

# if ENABLE_UNICODE_COMBINING_WCHARS || ENABLE_UNICODE_WIDE_WCHARS
	*width_adj += w;
#endif
	return wc;
}
#else /* !UNICODE */
static size_t load_string(const char *src, int maxsize)
{
	safe_strncpy(command_ps, src, maxsize);
	return strlen(command_ps);
}
# if ENABLE_FEATURE_TAB_COMPLETION
static void save_string(char *dst, unsigned maxsize)
{
	safe_strncpy(dst, command_ps, maxsize);
}
# endif
# define BB_PUTCHAR(c) bb_putchar(c)
/* Should never be called: */
int adjust_width_and_validate_wc(unsigned *width_adj, int wc);
#endif


/* Put 'command_ps[cursor]', cursor++.
 * Advance cursor on screen. If we reached right margin, scroll text up
 * and remove terminal margin effect by printing 'next_char' */
#define HACK_FOR_WRONG_WIDTH 1
static void put_cur_glyph_and_inc_cursor(void)
{
	CHAR_T c = command_ps[cursor];
	unsigned width = 0;
	int ofs_to_right;

	if (c == BB_NUL) {
		/* erase character after end of input string */
		c = ' ';
	} else {
		/* advance cursor only if we aren't at the end yet */
		cursor++;
		if (unicode_status == UNICODE_ON) {
			IF_UNICODE_WIDE_WCHARS(width = cmdedit_x;)
			c = adjust_width_and_validate_wc(&cmdedit_x, c);
			IF_UNICODE_WIDE_WCHARS(width = cmdedit_x - width;)
		} else {
			cmdedit_x++;
		}
	}

	ofs_to_right = cmdedit_x - cmdedit_termw;
	if (!ENABLE_UNICODE_WIDE_WCHARS || ofs_to_right <= 0) {
		/* c fits on this line */
		BB_PUTCHAR(c);
	}

	if (ofs_to_right >= 0) {
		/* we go to the next line */
#if HACK_FOR_WRONG_WIDTH
		/* This works better if our idea of term width is wrong
		 * and it is actually wider (often happens on serial lines).
		 * Printing CR,LF *forces* cursor to next line.
		 * OTOH if terminal width is correct AND terminal does NOT
		 * have automargin (IOW: it is moving cursor to next line
		 * by itself (which is wrong for VT-10x terminals)),
		 * this will break things: there will be one extra empty line */
		puts("\r"); /* + implicit '\n' */
#else
		/* VT-10x terminals don't wrap cursor to next line when last char
		 * on the line is printed - cursor stays "over" this char.
		 * Need to print _next_ char too (first one to appear on next line)
		 * to make cursor move down to next line.
		 */
		/* Works ok only if cmdedit_termw is correct. */
		c = command_ps[cursor];
		if (c == BB_NUL)
			c = ' ';
		BB_PUTCHAR(c);
		bb_putchar('\b');
#endif
		cmdedit_y++;
		if (!ENABLE_UNICODE_WIDE_WCHARS || ofs_to_right == 0) {
			width = 0;
		} else { /* ofs_to_right > 0 */
			/* wide char c didn't fit on prev line */
			BB_PUTCHAR(c);
		}
		cmdedit_x = width;
	}
}

/* Move to end of line (by printing all chars till the end) */
static void put_till_end_and_adv_cursor(void)
{
	while (cursor < command_len)
		put_cur_glyph_and_inc_cursor();
}

/* Go to the next line */
static void goto_new_line(void)
{
	put_till_end_and_adv_cursor();
	if (cmdedit_x != 0)
		bb_putchar('\n');
}

static void beep(void)
{
	bb_putchar('\007');
}

static void put_prompt(void)
{
	unsigned w;

	fputs(cmdedit_prompt, stdout);
	fflush_all();
	cursor = 0;
	w = cmdedit_termw; /* read volatile var once */
	cmdedit_y = cmdedit_prmt_len / w; /* new quasireal y */
	cmdedit_x = cmdedit_prmt_len % w;
}

/* Move back one character */
/* (optimized for slow terminals) */
static void input_backward(unsigned num)
{
	if (num > cursor)
		num = cursor;
	if (num == 0)
		return;
	cursor -= num;

	if ((ENABLE_UNICODE_COMBINING_WCHARS || ENABLE_UNICODE_WIDE_WCHARS)
	 && unicode_status == UNICODE_ON
	) {
		/* correct NUM to be equal to _screen_ width */
		int n = num;
		num = 0;
		while (--n >= 0)
			adjust_width_and_validate_wc(&num, command_ps[cursor + n]);
		if (num == 0)
			return;
	}

	if (cmdedit_x >= num) {
		cmdedit_x -= num;
		if (num <= 4) {
			/* This is longer by 5 bytes on x86.
			 * Also gets miscompiled for ARM users
			 * (busybox.net/bugs/view.php?id=2274).
			 * printf(("\b\b\b\b" + 4) - num);
			 * return;
			 */
			do {
				bb_putchar('\b');
			} while (--num);
			return;
		}
		printf("\033[%uD", num);
		return;
	}

	/* Need to go one or more lines up */
	if (ENABLE_UNICODE_WIDE_WCHARS) {
		/* With wide chars, it is hard to "backtrack"
		 * and reliably figure out where to put cursor.
		 * Example (<> is a wide char; # is an ordinary char, _ cursor):
		 * |prompt: <><> |
		 * |<><><><><><> |
		 * |_            |
		 * and user presses left arrow. num = 1, cmdedit_x = 0,
		 * We need to go up one line, and then - how do we know that
		 * we need to go *10* positions to the right? Because
		 * |prompt: <>#<>|
		 * |<><><>#<><><>|
		 * |_            |
		 * in this situation we need to go *11* positions to the right.
		 *
		 * A simpler thing to do is to redraw everything from the start
		 * up to new cursor position (which is already known):
		 */
		unsigned sv_cursor;
		/* go to 1st column; go up to first line */
		printf("\r" "\033[%uA", cmdedit_y);
		cmdedit_y = 0;
		sv_cursor = cursor;
		put_prompt(); /* sets cursor to 0 */
		while (cursor < sv_cursor)
			put_cur_glyph_and_inc_cursor();
	} else {
		int lines_up;
		unsigned width;
		/* num = chars to go back from the beginning of current line: */
		num -= cmdedit_x;
		width = cmdedit_termw; /* read volatile var once */
		/* num=1...w: one line up, w+1...2w: two, etc: */
		lines_up = 1 + (num - 1) / width;
		cmdedit_x = (width * cmdedit_y - num) % width;
		cmdedit_y -= lines_up;
		/* go to 1st column; go up */
		printf("\r" "\033[%uA", lines_up);
		/* go to correct column.
		 * xterm, konsole, Linux VT interpret 0 as 1 below! wow.
		 * need to *make sure* we skip it if cmdedit_x == 0 */
		if (cmdedit_x)
			printf("\033[%uC", cmdedit_x);
	}
}

/* draw prompt, editor line, and clear tail */
static void redraw(int y, int back_cursor)
{
	if (y > 0) /* up y lines */
		printf("\033[%uA", y);
	bb_putchar('\r');
	put_prompt();
	put_till_end_and_adv_cursor();
	printf(SEQ_CLEAR_TILL_END_OF_SCREEN);
	input_backward(back_cursor);
}

/* Delete the char in front of the cursor, optionally saving it
 * for later putback */
#if !ENABLE_FEATURE_EDITING_VI
static void input_delete(void)
#define input_delete(save) input_delete()
#else
static void input_delete(int save)
#endif
{
	int j = cursor;

	if (j == (int)command_len)
		return;

#if ENABLE_FEATURE_EDITING_VI
	if (save) {
		if (newdelflag) {
			delptr = delbuf;
			newdelflag = 0;
		}
		if ((delptr - delbuf) < DELBUFSIZ)
			*delptr++ = command_ps[j];
	}
#endif

	memmove(command_ps + j, command_ps + j + 1,
			/* (command_len + 1 [because of NUL]) - (j + 1)
			 * simplified into (command_len - j) */
			(command_len - j) * sizeof(command_ps[0]));
	command_len--;
	put_till_end_and_adv_cursor();
	/* Last char is still visible, erase it (and more) */
	printf(SEQ_CLEAR_TILL_END_OF_SCREEN);
	input_backward(cursor - j);     /* back to old pos cursor */
}

#if ENABLE_FEATURE_EDITING_VI
static void put(void)
{
	int ocursor;
	int j = delptr - delbuf;

	if (j == 0)
		return;
	ocursor = cursor;
	/* open hole and then fill it */
	memmove(command_ps + cursor + j, command_ps + cursor,
			(command_len - cursor + 1) * sizeof(command_ps[0]));
	memcpy(command_ps + cursor, delbuf, j * sizeof(command_ps[0]));
	command_len += j;
	put_till_end_and_adv_cursor();
	input_backward(cursor - ocursor - j + 1); /* at end of new text */
}
#endif

/* Delete the char in back of the cursor */
static void input_backspace(void)
{
	if (cursor > 0) {
		input_backward(1);
		input_delete(0);
	}
}

/* Move forward one character */
static void input_forward(void)
{
	if (cursor < command_len)
		put_cur_glyph_and_inc_cursor();
}

#if ENABLE_FEATURE_TAB_COMPLETION

static void free_tab_completion_data(void)
{
	if (matches) {
		while (num_matches)
			free(matches[--num_matches]);
		free(matches);
		matches = NULL;
	}
}

static void add_match(char *matched)
{
	matches = xrealloc_vector(matches, 4, num_matches);
	matches[num_matches] = matched;
	num_matches++;
}

#if ENABLE_FEATURE_USERNAME_COMPLETION
static void username_tab_completion(char *ud, char *with_shash_flg)
{
	struct passwd *entry;
	int userlen;

	ud++;                           /* ~user/... to user/... */
	userlen = strlen(ud);

	if (with_shash_flg) {           /* "~/..." or "~user/..." */
		char *sav_ud = ud - 1;
		char *home = NULL;

		if (*ud == '/') {       /* "~/..."     */
			home = home_pwd_buf;
		} else {
			/* "~user/..." */
			char *temp;
			temp = strchr(ud, '/');
			*temp = '\0';           /* ~user\0 */
			entry = getpwnam(ud);
			*temp = '/';            /* restore ~user/... */
			ud = temp;
			if (entry)
				home = entry->pw_dir;
		}
		if (home) {
			if ((userlen + strlen(home) + 1) < MAX_LINELEN) {
				/* /home/user/... */
				sprintf(sav_ud, "%s%s", home, ud);
			}
		}
	} else {
		/* "~[^/]*" */
		/* Using _r function to avoid pulling in static buffers */
		char line_buff[256];
		struct passwd pwd;
		struct passwd *result;

		setpwent();
		while (!getpwent_r(&pwd, line_buff, sizeof(line_buff), &result)) {
			/* Null usernames should result in all users as possible completions. */
			if (/*!userlen || */ strncmp(ud, pwd.pw_name, userlen) == 0) {
				add_match(xasprintf("~%s/", pwd.pw_name));
			}
		}
		endpwent();
	}
}
#endif  /* FEATURE_COMMAND_USERNAME_COMPLETION */

enum {
	FIND_EXE_ONLY = 0,
	FIND_DIR_ONLY = 1,
	FIND_FILE_ONLY = 2,
};

static int path_parse(char ***p, int flags)
{
	int npth;
	const char *pth;
	char *tmp;
	char **res;

	/* if not setenv PATH variable, to search cur dir "." */
	if (flags != FIND_EXE_ONLY)
		return 1;

	if (state->flags & WITH_PATH_LOOKUP)
		pth = state->path_lookup;
	else
		pth = getenv("PATH");
	/* PATH=<empty> or PATH=:<empty> */
	if (!pth || !pth[0] || LONE_CHAR(pth, ':'))
		return 1;

	tmp = (char*)pth;
	npth = 1; /* path component count */
	while (1) {
		tmp = strchr(tmp, ':');
		if (!tmp)
			break;
		if (*++tmp == '\0')
			break;  /* :<empty> */
		npth++;
	}

	res = xmalloc(npth * sizeof(char*));
	res[0] = tmp = xstrdup(pth);
	npth = 1;
	while (1) {
		tmp = strchr(tmp, ':');
		if (!tmp)
			break;
		*tmp++ = '\0'; /* ':' -> '\0' */
		if (*tmp == '\0')
			break; /* :<empty> */
		res[npth++] = tmp;
	}
	*p = res;
	return npth;
}

static void exe_n_cwd_tab_completion(char *command, int type)
{
	DIR *dir;
	struct dirent *next;
	struct stat st;
	char *path1[1];
	char **paths = path1;
	int npaths;
	int i;
	char *found;
	char *pfind = strrchr(command, '/');
/*	char dirbuf[MAX_LINELEN]; */
#define dirbuf (S.exe_n_cwd_tab_completion__dirbuf)

	npaths = 1;
	path1[0] = (char*)".";

	if (pfind == NULL) {
		/* no dir, if flags==EXE_ONLY - get paths, else "." */
		npaths = path_parse(&paths, type);
		pfind = command;
	} else {
		/* dirbuf = ".../.../.../" */
		safe_strncpy(dirbuf, command, (pfind - command) + 2);
#if ENABLE_FEATURE_USERNAME_COMPLETION
		if (dirbuf[0] == '~')   /* ~/... or ~user/... */
			username_tab_completion(dirbuf, dirbuf);
#endif
		paths[0] = dirbuf;
		/* point to 'l' in "..../last_component" */
		pfind++;
	}

	for (i = 0; i < npaths; i++) {
		dir = opendir(paths[i]);
		if (!dir)
			continue; /* don't print an error */

		while ((next = readdir(dir)) != NULL) {
			int len1;
			const char *str_found = next->d_name;

			/* matched? */
			if (strncmp(str_found, pfind, strlen(pfind)))
				continue;
			/* not see .name without .match */
			if (*str_found == '.' && *pfind == '\0') {
				if (NOT_LONE_CHAR(paths[i], '/') || str_found[1])
					continue;
				str_found = ""; /* only "/" */
			}
			found = concat_path_file(paths[i], str_found);
			/* hmm, remove in progress? */
			/* NB: stat() first so that we see is it a directory;
			 * but if that fails, use lstat() so that
			 * we still match dangling links */
			if (stat(found, &st) && lstat(found, &st))
				goto cont;
			/* find with dirs? */
			if (paths[i] != dirbuf)
				strcpy(found, next->d_name); /* only name */

			len1 = strlen(found);
			found = xrealloc(found, len1 + 2);
			found[len1] = '\0';
			found[len1+1] = '\0';

			if (S_ISDIR(st.st_mode)) {
				/* name is a directory */
				if (found[len1-1] != '/') {
					found[len1] = '/';
				}
			} else {
				/* not put found file if search only dirs for cd */
				if (type == FIND_DIR_ONLY)
					goto cont;
			}
			/* Add it to the list */
			add_match(found);
			continue;
 cont:
			free(found);
		}
		closedir(dir);
	}
	if (paths != path1) {
		free(paths[0]); /* allocated memory is only in first member */
		free(paths);
	}
#undef dirbuf
}

/* QUOT is used on elements of int_buf[], which are bytes,
 * not Unicode chars. Therefore it works correctly even in Unicode mode.
 */
#define QUOT (UCHAR_MAX+1)

#define int_buf (S.find_match__int_buf)
#define pos_buf (S.find_match__pos_buf)
/* is must be <= in */
static void collapse_pos(int is, int in)
{
	memmove(int_buf+is, int_buf+in, (MAX_LINELEN+1-in)*sizeof(int_buf[0]));
	memmove(pos_buf+is, pos_buf+in, (MAX_LINELEN+1-in)*sizeof(pos_buf[0]));
}
static NOINLINE int find_match(char *matchBuf, int *len_with_quotes)
{
	int i, j;
	int command_mode;
	int c, c2;
/*	Were local, but it uses too much stack */
/*	int16_t int_buf[MAX_LINELEN + 1]; */
/*	int16_t pos_buf[MAX_LINELEN + 1]; */

	/* set to integer dimension characters and own positions */
	for (i = 0;; i++) {
		int_buf[i] = (unsigned char)matchBuf[i];
		if (int_buf[i] == 0) {
			pos_buf[i] = -1; /* end-fo-line indicator */
			break;
		}
		pos_buf[i] = i;
	}

	/* mask \+symbol and convert '\t' to ' ' */
	for (i = j = 0; matchBuf[i]; i++, j++) {
		if (matchBuf[i] == '\\') {
			collapse_pos(j, j + 1);
			int_buf[j] |= QUOT;
			i++;
		}
	}
	/* mask "symbols" or 'symbols' */
	c2 = 0;
	for (i = 0; int_buf[i]; i++) {
		c = int_buf[i];
		if (c == '\'' || c == '"') {
			if (c2 == 0)
				c2 = c;
			else {
				if (c == c2)
					c2 = 0;
				else
					int_buf[i] |= QUOT;
			}
		} else if (c2 != 0 && c != '$')
			int_buf[i] |= QUOT;
	}

	/* skip commands with arguments if line has commands delimiters */
	/* ';' ';;' '&' '|' '&&' '||' but `>&' `<&' `>|' */
	for (i = 0; int_buf[i]; i++) {
		c = int_buf[i];
		c2 = int_buf[i + 1];
		j = i ? int_buf[i - 1] : -1;
		command_mode = 0;
		if (c == ';' || c == '&' || c == '|') {
			command_mode = 1 + (c == c2);
			if (c == '&') {
				if (j == '>' || j == '<')
					command_mode = 0;
			} else if (c == '|' && j == '>')
				command_mode = 0;
		}
		if (command_mode) {
			collapse_pos(0, i + command_mode);
			i = -1;  /* hack incremet */
		}
	}
	/* collapse `command...` */
	for (i = 0; int_buf[i]; i++) {
		if (int_buf[i] == '`') {
			for (j = i + 1; int_buf[j]; j++)
				if (int_buf[j] == '`') {
					collapse_pos(i, j + 1);
					j = 0;
					break;
				}
			if (j) {
				/* not found closing ` - command mode, collapse all previous */
				collapse_pos(0, i + 1);
				break;
			} else
				i--;  /* hack incremet */
		}
	}

	/* collapse (command...(command...)...) or {command...{command...}...} */
	c = 0;  /* "recursive" level */
	c2 = 0;
	for (i = 0; int_buf[i]; i++) {
		if (int_buf[i] == '(' || int_buf[i] == '{') {
			if (int_buf[i] == '(')
				c++;
			else
				c2++;
			collapse_pos(0, i + 1);
			i = -1;  /* hack incremet */
		}
	}
	for (i = 0; pos_buf[i] >= 0 && (c > 0 || c2 > 0); i++) {
		if ((int_buf[i] == ')' && c > 0) || (int_buf[i] == '}' && c2 > 0)) {
			if (int_buf[i] == ')')
				c--;
			else
				c2--;
			collapse_pos(0, i + 1);
			i = -1;  /* hack incremet */
		}
	}

	/* skip first not quote space */
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] != ' ')
			break;
	if (i)
		collapse_pos(0, i);

	/* set find mode for completion */
	command_mode = FIND_EXE_ONLY;
	for (i = 0; int_buf[i]; i++) {
		if (int_buf[i] == ' ' || int_buf[i] == '<' || int_buf[i] == '>') {
			if (int_buf[i] == ' ' && command_mode == FIND_EXE_ONLY
			 && matchBuf[pos_buf[0]] == 'c'
			 && matchBuf[pos_buf[1]] == 'd'
			) {
				command_mode = FIND_DIR_ONLY;
			} else {
				command_mode = FIND_FILE_ONLY;
				break;
			}
		}
	}
	for (i = 0; int_buf[i]; i++)
		/* "strlen" */;
	/* find last word */
	for (--i; i >= 0; i--) {
		c = int_buf[i];
		if (c == ' ' || c == '<' || c == '>' || c == '|' || c == '&') {
			collapse_pos(0, i + 1);
			break;
		}
	}
	/* skip first not quoted '\'' or '"' */
	for (i = 0; int_buf[i] == '\'' || int_buf[i] == '"'; i++)
		/*skip*/;
	/* collapse quote or unquote // or /~ */
	while ((int_buf[i] & ~QUOT) == '/'
	 && ((int_buf[i+1] & ~QUOT) == '/' || (int_buf[i+1] & ~QUOT) == '~')
	) {
		i++;
	}

	/* set only match and destroy quotes */
	j = 0;
	for (c = 0; pos_buf[i] >= 0; i++) {
		matchBuf[c++] = matchBuf[pos_buf[i]];
		j = pos_buf[i] + 1;
	}
	matchBuf[c] = '\0';
	/* old length matchBuf with quotes symbols */
	*len_with_quotes = j ? j - pos_buf[0] : 0;

	return command_mode;
}
#undef int_buf
#undef pos_buf

/*
 * display by column (original idea from ls applet,
 * very optimized by me :)
 */
static void showfiles(void)
{
	int ncols, row;
	int column_width = 0;
	int nfiles = num_matches;
	int nrows = nfiles;
	int l;

	/* find the longest file name - use that as the column width */
	for (row = 0; row < nrows; row++) {
		l = unicode_strwidth(matches[row]);
		if (column_width < l)
			column_width = l;
	}
	column_width += 2;              /* min space for columns */
	ncols = cmdedit_termw / column_width;

	if (ncols > 1) {
		nrows /= ncols;
		if (nfiles % ncols)
			nrows++;        /* round up fractionals */
	} else {
		ncols = 1;
	}
	for (row = 0; row < nrows; row++) {
		int n = row;
		int nc;

		for (nc = 1; nc < ncols && n+nrows < nfiles; n += nrows, nc++) {
			printf("%s%-*s", matches[n],
				(int)(column_width - unicode_strwidth(matches[n])), ""
			);
		}
		if (ENABLE_UNICODE_SUPPORT)
			puts(printable_string(NULL, matches[n]));
		else
			puts(matches[n]);
	}
}

static char *add_quote_for_spec_chars(char *found)
{
	int l = 0;
	char *s = xzalloc((strlen(found) + 1) * 2);

	while (*found) {
		if (strchr(" `\"#$%^&*()=+{}[]:;'|\\<>", *found))
			s[l++] = '\\';
		s[l++] = *found++;
	}
	/* s[l] = '\0'; - already is */
	return s;
}

/* Do TAB completion */
static void input_tab(smallint *lastWasTab)
{
	if (!(state->flags & TAB_COMPLETION))
		return;

	if (!*lastWasTab) {
		char *tmp, *tmp1;
		size_t len_found;
/*		char matchBuf[MAX_LINELEN]; */
#define matchBuf (S.input_tab__matchBuf)
		int find_type;
		int recalc_pos;
#if ENABLE_UNICODE_SUPPORT
		/* cursor pos in command converted to multibyte form */
		int cursor_mb;
#endif

		*lastWasTab = TRUE;             /* flop trigger */

		/* Make a local copy of the string --
		 * up to the position of the cursor */
		save_string(matchBuf, cursor + 1);
#if ENABLE_UNICODE_SUPPORT
		cursor_mb = strlen(matchBuf);
#endif
		tmp = matchBuf;

		find_type = find_match(matchBuf, &recalc_pos);

		/* Free up any memory already allocated */
		free_tab_completion_data();

#if ENABLE_FEATURE_USERNAME_COMPLETION
		/* If the word starts with `~' and there is no slash in the word,
		 * then try completing this word as a username. */
		if (state->flags & USERNAME_COMPLETION)
			if (matchBuf[0] == '~' && strchr(matchBuf, '/') == NULL)
				username_tab_completion(matchBuf, NULL);
#endif
		/* Try to match any executable in our path and everything
		 * in the current working directory */
		if (!matches)
			exe_n_cwd_tab_completion(matchBuf, find_type);
		/* Sort, then remove any duplicates found */
		if (matches) {
			unsigned i;
			int n = 0;
			qsort_string_vector(matches, num_matches);
			for (i = 0; i < num_matches - 1; ++i) {
				if (matches[i] && matches[i+1]) { /* paranoia */
					if (strcmp(matches[i], matches[i+1]) == 0) {
						free(matches[i]);
						matches[i] = NULL; /* paranoia */
					} else {
						matches[n++] = matches[i];
					}
				}
			}
			matches[n] = matches[i];
			num_matches = n + 1;
		}
		/* Did we find exactly one match? */
		if (!matches || num_matches > 1) { /* no */
			beep();
			if (!matches)
				return;         /* not found */
			/* find minimal match */
			tmp1 = xstrdup(matches[0]);
			for (tmp = tmp1; *tmp; tmp++) {
				for (len_found = 1; len_found < num_matches; len_found++) {
					if (matches[len_found][tmp - tmp1] != *tmp) {
						*tmp = '\0';
						break;
					}
				}
			}
			if (*tmp1 == '\0') {        /* have unique */
				free(tmp1);
				return;
			}
			tmp = add_quote_for_spec_chars(tmp1);
			free(tmp1);
		} else {                        /* one match */
			tmp = add_quote_for_spec_chars(matches[0]);
			/* for next completion current found */
			*lastWasTab = FALSE;

			len_found = strlen(tmp);
			if (tmp[len_found-1] != '/') {
				tmp[len_found] = ' ';
				tmp[len_found+1] = '\0';
			}
		}

		len_found = strlen(tmp);
#if !ENABLE_UNICODE_SUPPORT
		/* have space to place the match? */
		/* The result consists of three parts with these lengths: */
		/* (cursor - recalc_pos) + len_found + (command_len - cursor) */
		/* it simplifies into: */
		if ((int)(len_found + command_len - recalc_pos) < S.maxsize) {
			/* save tail */
			strcpy(matchBuf, command_ps + cursor);
			/* add match and tail */
			sprintf(&command_ps[cursor - recalc_pos], "%s%s", tmp, matchBuf);
			command_len = strlen(command_ps);
			/* new pos */
			recalc_pos = cursor - recalc_pos + len_found;
			/* write out the matched command */
			redraw(cmdedit_y, command_len - recalc_pos);
		}
#else
		{
			char command[MAX_LINELEN];
			int len = save_string(command, sizeof(command));
			/* have space to place the match? */
			/* (cursor_mb - recalc_pos) + len_found + (len - cursor_mb) */
			if ((int)(len_found + len - recalc_pos) < MAX_LINELEN) {
				/* save tail */
				strcpy(matchBuf, command + cursor_mb);
				/* where do we want to have cursor after all? */
				strcpy(&command[cursor_mb - recalc_pos], tmp);
				len = load_string(command, S.maxsize);
				/* add match and tail */
				sprintf(&command[cursor_mb - recalc_pos], "%s%s", tmp, matchBuf);
				command_len = load_string(command, S.maxsize);
				/* write out the matched command */
				redraw(cmdedit_y, command_len - len);
			}
		}
#endif
		free(tmp);
#undef matchBuf
	} else {
		/* Ok -- the last char was a TAB.  Since they
		 * just hit TAB again, print a list of all the
		 * available choices... */
		if (matches && num_matches > 0) {
			/* changed by goto_new_line() */
			int sav_cursor = cursor;

			/* Go to the next line */
			goto_new_line();
			showfiles();
			redraw(0, command_len - sav_cursor);
		}
	}
}

#endif  /* FEATURE_COMMAND_TAB_COMPLETION */


line_input_t* FAST_FUNC new_line_input_t(int flags)
{
	line_input_t *n = xzalloc(sizeof(*n));
	n->flags = flags;
	return n;
}


#if MAX_HISTORY > 0

static void save_command_ps_at_cur_history(void)
{
	if (command_ps[0] != BB_NUL) {
		int cur = state->cur_history;
		free(state->history[cur]);

# if ENABLE_UNICODE_SUPPORT
		{
			char tbuf[MAX_LINELEN];
			save_string(tbuf, sizeof(tbuf));
			state->history[cur] = xstrdup(tbuf);
		}
# else
		state->history[cur] = xstrdup(command_ps);
# endif
	}
}

/* state->flags is already checked to be nonzero */
static int get_previous_history(void)
{
	if ((state->flags & DO_HISTORY) && state->cur_history) {
		save_command_ps_at_cur_history();
		state->cur_history--;
		return 1;
	}
	beep();
	return 0;
}

static int get_next_history(void)
{
	if (state->flags & DO_HISTORY) {
		if (state->cur_history < state->cnt_history) {
			save_command_ps_at_cur_history(); /* save the current history line */
			return ++state->cur_history;
		}
	}
	beep();
	return 0;
}

# if ENABLE_FEATURE_EDITING_SAVEHISTORY
/* We try to ensure that concurrent additions to the history
 * do not overwrite each other.
 * Otherwise shell users get unhappy.
 *
 * History file is trimmed lazily, when it grows several times longer
 * than configured MAX_HISTORY lines.
 */

static void free_line_input_t(line_input_t *n)
{
	int i = n->cnt_history;
	while (i > 0)
		free(n->history[--i]);
	free(n);
}

/* state->flags is already checked to be nonzero */
static void load_history(line_input_t *st_parm)
{
	char *temp_h[MAX_HISTORY];
	char *line;
	FILE *fp;
	unsigned idx, i, line_len;

	/* NB: do not trash old history if file can't be opened */

	fp = fopen_for_read(st_parm->hist_file);
	if (fp) {
		/* clean up old history */
		for (idx = st_parm->cnt_history; idx > 0;) {
			idx--;
			free(st_parm->history[idx]);
			st_parm->history[idx] = NULL;
		}

		/* fill temp_h[], retaining only last MAX_HISTORY lines */
		memset(temp_h, 0, sizeof(temp_h));
		st_parm->cnt_history_in_file = idx = 0;
		while ((line = xmalloc_fgetline(fp)) != NULL) {
			if (line[0] == '\0') {
				free(line);
				continue;
			}
			free(temp_h[idx]);
			temp_h[idx] = line;
			st_parm->cnt_history_in_file++;
			idx++;
			if (idx == MAX_HISTORY)
				idx = 0;
		}
		fclose(fp);

		/* find first non-NULL temp_h[], if any */
		if (st_parm->cnt_history_in_file) {
			while (temp_h[idx] == NULL) {
				idx++;
				if (idx == MAX_HISTORY)
					idx = 0;
			}
		}

		/* copy temp_h[] to st_parm->history[] */
		for (i = 0; i < MAX_HISTORY;) {
			line = temp_h[idx];
			if (!line)
				break;
			idx++;
			if (idx == MAX_HISTORY)
				idx = 0;
			line_len = strlen(line);
			if (line_len >= MAX_LINELEN)
				line[MAX_LINELEN-1] = '\0';
			st_parm->history[i++] = line;
		}
		st_parm->cnt_history = i;
	}
}

/* state->flags is already checked to be nonzero */
static void save_history(char *str)
{
	int fd;
	int len, len2;

	fd = open(state->hist_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0)
		return;
	xlseek(fd, 0, SEEK_END); /* paranoia */
	len = strlen(str);
	str[len] = '\n'; /* we (try to) do atomic write */
	len2 = full_write(fd, str, len + 1);
	str[len] = '\0';
	close(fd);
	if (len2 != len + 1)
		return; /* "wtf?" */

	/* did we write so much that history file needs trimming? */
	state->cnt_history_in_file++;
	if (state->cnt_history_in_file > MAX_HISTORY * 4) {
		FILE *fp;
		char *new_name;
		line_input_t *st_temp;
		int i;

		/* we may have concurrently written entries from others.
		 * load them */
		st_temp = new_line_input_t(state->flags);
		st_temp->hist_file = state->hist_file;
		load_history(st_temp);

		/* write out temp file and replace hist_file atomically */
		new_name = xasprintf("%s.%u.new", state->hist_file, (int) getpid());
		fp = fopen_for_write(new_name);
		if (fp) {
			for (i = 0; i < st_temp->cnt_history; i++)
				fprintf(fp, "%s\n", st_temp->history[i]);
			fclose(fp);
			if (rename(new_name, state->hist_file) == 0)
				state->cnt_history_in_file = st_temp->cnt_history;
		}
		free(new_name);
		free_line_input_t(st_temp);
	}
}
# else
#  define load_history(a) ((void)0)
#  define save_history(a) ((void)0)
# endif /* FEATURE_COMMAND_SAVEHISTORY */

static void remember_in_history(char *str)
{
	int i;

	if (!(state->flags & DO_HISTORY))
		return;
	if (str[0] == '\0')
		return;
	i = state->cnt_history;
	/* Don't save dupes */
	if (i && strcmp(state->history[i-1], str) == 0)
		return;

	free(state->history[MAX_HISTORY]); /* redundant, paranoia */
	state->history[MAX_HISTORY] = NULL; /* redundant, paranoia */

	/* If history[] is full, remove the oldest command */
	/* we need to keep history[MAX_HISTORY] empty, hence >=, not > */
	if (i >= MAX_HISTORY) {
		free(state->history[0]);
		for (i = 0; i < MAX_HISTORY-1; i++)
			state->history[i] = state->history[i+1];
		/* i == MAX_HISTORY-1 */
	}
	/* i <= MAX_HISTORY-1 */
	state->history[i++] = xstrdup(str);
	/* i <= MAX_HISTORY */
	state->cur_history = i;
	state->cnt_history = i;
# if MAX_HISTORY > 0 && ENABLE_FEATURE_EDITING_SAVEHISTORY
	if ((state->flags & SAVE_HISTORY) && state->hist_file)
		save_history(str);
# endif
	IF_FEATURE_EDITING_FANCY_PROMPT(num_ok_lines++;)
}

#else /* MAX_HISTORY == 0 */
# define remember_in_history(a) ((void)0)
#endif /* MAX_HISTORY */


#if ENABLE_FEATURE_EDITING_VI
/*
 * vi mode implemented 2005 by Paul Fox <pgf@foxharp.boston.ma.us>
 */
static void
vi_Word_motion(int eat)
{
	CHAR_T *command = command_ps;

	while (cursor < command_len && !BB_isspace(command[cursor]))
		input_forward();
	if (eat) while (cursor < command_len && BB_isspace(command[cursor]))
		input_forward();
}

static void
vi_word_motion(int eat)
{
	CHAR_T *command = command_ps;

	if (BB_isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor < command_len
		 && (BB_isalnum(command[cursor+1]) || command[cursor+1] == '_')
		) {
			input_forward();
		}
	} else if (BB_ispunct(command[cursor])) {
		while (cursor < command_len && BB_ispunct(command[cursor+1]))
			input_forward();
	}

	if (cursor < command_len)
		input_forward();

	if (eat) {
		while (cursor < command_len && BB_isspace(command[cursor]))
			input_forward();
	}
}

static void
vi_End_motion(void)
{
	CHAR_T *command = command_ps;

	input_forward();
	while (cursor < command_len && BB_isspace(command[cursor]))
		input_forward();
	while (cursor < command_len-1 && !BB_isspace(command[cursor+1]))
		input_forward();
}

static void
vi_end_motion(void)
{
	CHAR_T *command = command_ps;

	if (cursor >= command_len-1)
		return;
	input_forward();
	while (cursor < command_len-1 && BB_isspace(command[cursor]))
		input_forward();
	if (cursor >= command_len-1)
		return;
	if (BB_isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor < command_len-1
		 && (BB_isalnum(command[cursor+1]) || command[cursor+1] == '_')
		) {
			input_forward();
		}
	} else if (BB_ispunct(command[cursor])) {
		while (cursor < command_len-1 && BB_ispunct(command[cursor+1]))
			input_forward();
	}
}

static void
vi_Back_motion(void)
{
	CHAR_T *command = command_ps;

	while (cursor > 0 && BB_isspace(command[cursor-1]))
		input_backward(1);
	while (cursor > 0 && !BB_isspace(command[cursor-1]))
		input_backward(1);
}

static void
vi_back_motion(void)
{
	CHAR_T *command = command_ps;

	if (cursor <= 0)
		return;
	input_backward(1);
	while (cursor > 0 && BB_isspace(command[cursor]))
		input_backward(1);
	if (cursor <= 0)
		return;
	if (BB_isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor > 0
		 && (BB_isalnum(command[cursor-1]) || command[cursor-1] == '_')
		) {
			input_backward(1);
		}
	} else if (BB_ispunct(command[cursor])) {
		while (cursor > 0 && BB_ispunct(command[cursor-1]))
			input_backward(1);
	}
}
#endif

/* Modelled after bash 4.0 behavior of Ctrl-<arrow> */
static void ctrl_left(void)
{
	CHAR_T *command = command_ps;

	while (1) {
		CHAR_T c;

		input_backward(1);
		if (cursor == 0)
			break;
		c = command[cursor];
		if (c != ' ' && !BB_ispunct(c)) {
			/* we reached a "word" delimited by spaces/punct.
			 * go to its beginning */
			while (1) {
				c = command[cursor - 1];
				if (c == ' ' || BB_ispunct(c))
					break;
				input_backward(1);
				if (cursor == 0)
					break;
			}
			break;
		}
	}
}
static void ctrl_right(void)
{
	CHAR_T *command = command_ps;

	while (1) {
		CHAR_T c;

		c = command[cursor];
		if (c == BB_NUL)
			break;
		if (c != ' ' && !BB_ispunct(c)) {
			/* we reached a "word" delimited by spaces/punct.
			 * go to its end + 1 */
			while (1) {
				input_forward();
				c = command[cursor];
				if (c == BB_NUL || c == ' ' || BB_ispunct(c))
					break;
			}
			break;
		}
		input_forward();
	}
}


/*
 * read_line_input and its helpers
 */

#if ENABLE_FEATURE_EDITING_ASK_TERMINAL
static void ask_terminal(void)
{
	/* Ask terminal where is the cursor now.
	 * lineedit_read_key handles response and corrects
	 * our idea of current cursor position.
	 * Testcase: run "echo -n long_line_long_line_long_line",
	 * then type in a long, wrapping command and try to
	 * delete it using backspace key.
	 * Note: we print it _after_ prompt, because
	 * prompt may contain CR. Example: PS1='\[\r\n\]\w '
	 */
	/* Problem: if there is buffered input on stdin,
	 * the response will be delivered later,
	 * possibly to an unsuspecting application.
	 * Testcase: "sleep 1; busybox ash" + press and hold [Enter].
	 * Result:
	 * ~/srcdevel/bbox/fix/busybox.t4 #
	 * ~/srcdevel/bbox/fix/busybox.t4 #
	 * ^[[59;34~/srcdevel/bbox/fix/busybox.t4 #  <-- garbage
	 * ~/srcdevel/bbox/fix/busybox.t4 #
	 *
	 * Checking for input with poll only makes the race narrower,
	 * I still can trigger it. Strace:
	 *
	 * write(1, "~/srcdevel/bbox/fix/busybox.t4 # ", 33) = 33
	 * poll([{fd=0, events=POLLIN}], 1, 0) = 0 (Timeout)  <-- no input exists
	 * write(1, "\33[6n", 4) = 4  <-- send the ESC sequence, quick!
	 * poll([{fd=0, events=POLLIN}], 1, 4294967295) = 1 ([{fd=0, revents=POLLIN}])
	 * read(0, "\n", 1)      = 1  <-- oh crap, user's input got in first
	 */
	struct pollfd pfd;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	if (safe_poll(&pfd, 1, 0) == 0) {
		S.sent_ESC_br6n = 1;
		fputs("\033" "[6n", stdout);
		fflush_all(); /* make terminal see it ASAP! */
	}
}
#else
#define ask_terminal() ((void)0)
#endif

#if !ENABLE_FEATURE_EDITING_FANCY_PROMPT
static void parse_and_put_prompt(const char *prmt_ptr)
{
	cmdedit_prompt = prmt_ptr;
	cmdedit_prmt_len = strlen(prmt_ptr);
	put_prompt();
}
#else
static void parse_and_put_prompt(const char *prmt_ptr)
{
	int prmt_len = 0;
	size_t cur_prmt_len = 0;
	char flg_not_length = '[';
	char *prmt_mem_ptr = xzalloc(1);
	char *cwd_buf = xrealloc_getcwd_or_warn(NULL);
	char cbuf[2];
	char c;
	char *pbuf;

	cmdedit_prmt_len = 0;

	if (!cwd_buf) {
		cwd_buf = (char *)bb_msg_unknown;
	}

	cbuf[1] = '\0'; /* never changes */

	while (*prmt_ptr) {
		char *free_me = NULL;

		pbuf = cbuf;
		c = *prmt_ptr++;
		if (c == '\\') {
			const char *cp = prmt_ptr;
			int l;

			c = bb_process_escape_sequence(&prmt_ptr);
			if (prmt_ptr == cp) {
				if (*cp == '\0')
					break;
				c = *prmt_ptr++;

				switch (c) {
# if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
				case 'u':
					pbuf = user_buf ? user_buf : (char*)"";
					break;
# endif
				case 'h':
					pbuf = free_me = safe_gethostname();
					*strchrnul(pbuf, '.') = '\0';
					break;
				case '$':
					c = (geteuid() == 0 ? '#' : '$');
					break;
# if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
				case 'w':
					/* /home/user[/something] -> ~[/something] */
					pbuf = cwd_buf;
					l = strlen(home_pwd_buf);
					if (l != 0
					 && strncmp(home_pwd_buf, cwd_buf, l) == 0
					 && (cwd_buf[l]=='/' || cwd_buf[l]=='\0')
					 && strlen(cwd_buf + l) < PATH_MAX
					) {
						pbuf = free_me = xasprintf("~%s", cwd_buf + l);
					}
					break;
# endif
				case 'W':
					pbuf = cwd_buf;
					cp = strrchr(pbuf, '/');
					if (cp != NULL && cp != pbuf)
						pbuf += (cp-pbuf) + 1;
					break;
				case '!':
					pbuf = free_me = xasprintf("%d", num_ok_lines);
					break;
				case 'e': case 'E':     /* \e \E = \033 */
					c = '\033';
					break;
				case 'x': case 'X': {
					char buf2[4];
					for (l = 0; l < 3;) {
						unsigned h;
						buf2[l++] = *prmt_ptr;
						buf2[l] = '\0';
						h = strtoul(buf2, &pbuf, 16);
						if (h > UCHAR_MAX || (pbuf - buf2) < l) {
							buf2[--l] = '\0';
							break;
						}
						prmt_ptr++;
					}
					c = (char)strtoul(buf2, NULL, 16);
					if (c == 0)
						c = '?';
					pbuf = cbuf;
					break;
				}
				case '[': case ']':
					if (c == flg_not_length) {
						flg_not_length = (flg_not_length == '[' ? ']' : '[');
						continue;
					}
					break;
				} /* switch */
			} /* if */
		} /* if */
		cbuf[0] = c;
		cur_prmt_len = strlen(pbuf);
		prmt_len += cur_prmt_len;
		if (flg_not_length != ']')
			cmdedit_prmt_len += cur_prmt_len;
		prmt_mem_ptr = strcat(xrealloc(prmt_mem_ptr, prmt_len+1), pbuf);
		free(free_me);
	} /* while */

	if (cwd_buf != (char *)bb_msg_unknown)
		free(cwd_buf);
	cmdedit_prompt = prmt_mem_ptr;
	put_prompt();
}
#endif

static void cmdedit_setwidth(unsigned w, int redraw_flg)
{
	cmdedit_termw = w;
	if (redraw_flg) {
		/* new y for current cursor */
		int new_y = (cursor + cmdedit_prmt_len) / w;
		/* redraw */
		redraw((new_y >= cmdedit_y ? new_y : cmdedit_y), command_len - cursor);
		fflush_all();
	}
}

static void win_changed(int nsig)
{
	int sv_errno = errno;
	unsigned width;
	get_terminal_width_height(0, &width, NULL);
	cmdedit_setwidth(width, nsig /* - just a yes/no flag */);
	if (nsig == SIGWINCH)
		signal(SIGWINCH, win_changed); /* rearm ourself */
	errno = sv_errno;
}

static int lineedit_read_key(char *read_key_buffer)
{
	int64_t ic;
	int timeout = -1;
#if ENABLE_UNICODE_SUPPORT
	char unicode_buf[MB_CUR_MAX + 1];
	int unicode_idx = 0;
#endif

	while (1) {
		/* Wait for input. TIMEOUT = -1 makes read_key wait even
		 * on nonblocking stdin, TIMEOUT = 50 makes sure we won't
		 * insist on full MB_CUR_MAX buffer to declare input like
		 * "\xff\n",pause,"ls\n" invalid and thus won't lose "ls".
		 *
		 * Note: read_key sets errno to 0 on success.
		 */
		ic = read_key(STDIN_FILENO, read_key_buffer, timeout);
		if (errno) {
#if ENABLE_UNICODE_SUPPORT
			if (errno == EAGAIN && unicode_idx != 0)
				goto pushback;
#endif
			break;
		}

#if ENABLE_FEATURE_EDITING_ASK_TERMINAL
		if ((int32_t)ic == KEYCODE_CURSOR_POS
		 && S.sent_ESC_br6n
		) {
			S.sent_ESC_br6n = 0;
			if (cursor == 0) { /* otherwise it may be bogus */
				int col = ((ic >> 32) & 0x7fff) - 1;
				if (col > cmdedit_prmt_len) {
					cmdedit_x += (col - cmdedit_prmt_len);
					while (cmdedit_x >= cmdedit_termw) {
						cmdedit_x -= cmdedit_termw;
						cmdedit_y++;
					}
				}
			}
			continue;
		}
#endif

#if ENABLE_UNICODE_SUPPORT
		if (unicode_status == UNICODE_ON) {
			wchar_t wc;

			if ((int32_t)ic < 0) /* KEYCODE_xxx */
				break;
			// TODO: imagine sequence like: 0xff,<left-arrow>: we are currently losing 0xff...

			unicode_buf[unicode_idx++] = ic;
			unicode_buf[unicode_idx] = '\0';
			if (mbstowcs(&wc, unicode_buf, 1) != 1) {
				/* Not (yet?) a valid unicode char */
				if (unicode_idx < MB_CUR_MAX) {
					timeout = 50;
					continue;
				}
 pushback:
				/* Invalid sequence. Save all "bad bytes" except first */
				read_key_ungets(read_key_buffer, unicode_buf + 1, unicode_idx - 1);
# if !ENABLE_UNICODE_PRESERVE_BROKEN
				ic = CONFIG_SUBST_WCHAR;
# else
				ic = unicode_mark_raw_byte(unicode_buf[0]);
# endif
			} else {
				/* Valid unicode char, return its code */
				ic = wc;
			}
		}
#endif
		break;
	}

	return ic;
}

#if ENABLE_UNICODE_BIDI_SUPPORT
static int isrtl_str(void)
{
	int idx = cursor;

	while (idx < command_len && unicode_bidi_is_neutral_wchar(command_ps[idx]))
		idx++;
	return unicode_bidi_isrtl(command_ps[idx]);
}
#else
# define isrtl_str() 0
#endif

/* leave out the "vi-mode"-only case labels if vi editing isn't
 * configured. */
#define vi_case(caselabel) IF_FEATURE_EDITING_VI(case caselabel)

/* convert uppercase ascii to equivalent control char, for readability */
#undef CTRL
#define CTRL(a) ((a) & ~0x40)

/* maxsize must be >= 2.
 * Returns:
 * -1 on read errors or EOF, or on bare Ctrl-D,
 * 0  on ctrl-C (the line entered is still returned in 'command'),
 * >0 length of input string, including terminating '\n'
 */
int FAST_FUNC read_line_input(const char *prompt, char *command, int maxsize, line_input_t *st)
{
	int len;
#if ENABLE_FEATURE_TAB_COMPLETION
	smallint lastWasTab = FALSE;
#endif
	smallint break_out = 0;
#if ENABLE_FEATURE_EDITING_VI
	smallint vi_cmdmode = 0;
#endif
	struct termios initial_settings;
	struct termios new_settings;
	char read_key_buffer[KEYCODE_BUFFER_SIZE];

	INIT_S();

	if (tcgetattr(STDIN_FILENO, &initial_settings) < 0
	 || !(initial_settings.c_lflag & ECHO)
	) {
		/* Happens when e.g. stty -echo was run before */
		parse_and_put_prompt(prompt);
		/* fflush_all(); - done by parse_and_put_prompt */
		if (fgets(command, maxsize, stdin) == NULL)
			len = -1; /* EOF or error */
		else
			len = strlen(command);
		DEINIT_S();
		return len;
	}

	init_unicode();

// FIXME: audit & improve this
	if (maxsize > MAX_LINELEN)
		maxsize = MAX_LINELEN;
	S.maxsize = maxsize;

	/* With null flags, no other fields are ever used */
	state = st ? st : (line_input_t*) &const_int_0;
#if MAX_HISTORY > 0
# if ENABLE_FEATURE_EDITING_SAVEHISTORY
	if ((state->flags & SAVE_HISTORY) && state->hist_file)
		if (state->cnt_history == 0)
			load_history(state);
# endif
	if (state->flags & DO_HISTORY)
		state->cur_history = state->cnt_history;
#endif

	/* prepare before init handlers */
	cmdedit_y = 0;  /* quasireal y, not true if line > xt*yt */
	command_len = 0;
#if ENABLE_UNICODE_SUPPORT
	command_ps = xzalloc(maxsize * sizeof(command_ps[0]));
#else
	command_ps = command;
	command[0] = '\0';
#endif
#define command command_must_not_be_used

	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;        /* unbuffered input */
	/* Turn off echoing and CTRL-C, so we can trap it */
	new_settings.c_lflag &= ~(ECHO | ECHONL | ISIG);
	/* Hmm, in linux c_cc[] is not parsed if ICANON is off */
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	/* Turn off CTRL-C, so we can trap it */
#ifndef _POSIX_VDISABLE
# define _POSIX_VDISABLE '\0'
#endif
	new_settings.c_cc[VINTR] = _POSIX_VDISABLE;
	tcsetattr_stdin_TCSANOW(&new_settings);

	/* Now initialize things */
	previous_SIGWINCH_handler = signal(SIGWINCH, win_changed);
	win_changed(0); /* do initial resizing */
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	{
		struct passwd *entry;

		entry = getpwuid(geteuid());
		if (entry) {
			user_buf = xstrdup(entry->pw_name);
			home_pwd_buf = xstrdup(entry->pw_dir);
		}
	}
#endif

#if 0
	for (i = 0; i <= MAX_HISTORY; i++)
		bb_error_msg("history[%d]:'%s'", i, state->history[i]);
	bb_error_msg("cur_history:%d cnt_history:%d", state->cur_history, state->cnt_history);
#endif

	/* Print out the command prompt, optionally ask where cursor is */
	parse_and_put_prompt(prompt);
	ask_terminal();

	read_key_buffer[0] = 0;
	while (1) {
		/*
		 * The emacs and vi modes share much of the code in the big
		 * command loop.  Commands entered when in vi's command mode
		 * (aka "escape mode") get an extra bit added to distinguish
		 * them - this keeps them from being self-inserted. This
		 * clutters the big switch a bit, but keeps all the code
		 * in one place.
		 */
		enum {
			VI_CMDMODE_BIT = 0x40000000,
			/* 0x80000000 bit flags KEYCODE_xxx */
		};
		int32_t ic, ic_raw;

		fflush_all();
		ic = ic_raw = lineedit_read_key(read_key_buffer);

#if ENABLE_FEATURE_EDITING_VI
		newdelflag = 1;
		if (vi_cmdmode) {
			/* btw, since KEYCODE_xxx are all < 0, this doesn't
			 * change ic if it contains one of them: */
			ic |= VI_CMDMODE_BIT;
		}
#endif

		switch (ic) {
		case '\n':
		case '\r':
		vi_case('\n'|VI_CMDMODE_BIT:)
		vi_case('\r'|VI_CMDMODE_BIT:)
			/* Enter */
			goto_new_line();
			break_out = 1;
			break;
		case CTRL('A'):
		vi_case('0'|VI_CMDMODE_BIT:)
			/* Control-a -- Beginning of line */
			input_backward(cursor);
			break;
		case CTRL('B'):
		vi_case('h'|VI_CMDMODE_BIT:)
		vi_case('\b'|VI_CMDMODE_BIT:) /* ^H */
		vi_case('\x7f'|VI_CMDMODE_BIT:) /* DEL */
			input_backward(1); /* Move back one character */
			break;
		case CTRL('E'):
		vi_case('$'|VI_CMDMODE_BIT:)
			/* Control-e -- End of line */
			put_till_end_and_adv_cursor();
			break;
		case CTRL('F'):
		vi_case('l'|VI_CMDMODE_BIT:)
		vi_case(' '|VI_CMDMODE_BIT:)
			input_forward(); /* Move forward one character */
			break;
		case '\b':   /* ^H */
		case '\x7f': /* DEL */
			if (!isrtl_str())
				input_backspace();
			else
				input_delete(0);
			break;
		case KEYCODE_DELETE:
			if (!isrtl_str())
				input_delete(0);
			else
				input_backspace();
			break;
#if ENABLE_FEATURE_TAB_COMPLETION
		case '\t':
			input_tab(&lastWasTab);
			break;
#endif
		case CTRL('K'):
			/* Control-k -- clear to end of line */
			command_ps[cursor] = BB_NUL;
			command_len = cursor;
			printf(SEQ_CLEAR_TILL_END_OF_SCREEN);
			break;
		case CTRL('L'):
		vi_case(CTRL('L')|VI_CMDMODE_BIT:)
			/* Control-l -- clear screen */
			printf("\033[H"); /* cursor to top,left */
			redraw(0, command_len - cursor);
			break;
#if MAX_HISTORY > 0
		case CTRL('N'):
		vi_case(CTRL('N')|VI_CMDMODE_BIT:)
		vi_case('j'|VI_CMDMODE_BIT:)
			/* Control-n -- Get next command in history */
			if (get_next_history())
				goto rewrite_line;
			break;
		case CTRL('P'):
		vi_case(CTRL('P')|VI_CMDMODE_BIT:)
		vi_case('k'|VI_CMDMODE_BIT:)
			/* Control-p -- Get previous command from history */
			if (get_previous_history())
				goto rewrite_line;
			break;
#endif
		case CTRL('U'):
		vi_case(CTRL('U')|VI_CMDMODE_BIT:)
			/* Control-U -- Clear line before cursor */
			if (cursor) {
				command_len -= cursor;
				memmove(command_ps, command_ps + cursor,
					(command_len + 1) * sizeof(command_ps[0]));
				redraw(cmdedit_y, command_len);
			}
			break;
		case CTRL('W'):
		vi_case(CTRL('W')|VI_CMDMODE_BIT:)
			/* Control-W -- Remove the last word */
			while (cursor > 0 && BB_isspace(command_ps[cursor-1]))
				input_backspace();
			while (cursor > 0 && !BB_isspace(command_ps[cursor-1]))
				input_backspace();
			break;

#if ENABLE_FEATURE_EDITING_VI
		case 'i'|VI_CMDMODE_BIT:
			vi_cmdmode = 0;
			break;
		case 'I'|VI_CMDMODE_BIT:
			input_backward(cursor);
			vi_cmdmode = 0;
			break;
		case 'a'|VI_CMDMODE_BIT:
			input_forward();
			vi_cmdmode = 0;
			break;
		case 'A'|VI_CMDMODE_BIT:
			put_till_end_and_adv_cursor();
			vi_cmdmode = 0;
			break;
		case 'x'|VI_CMDMODE_BIT:
			input_delete(1);
			break;
		case 'X'|VI_CMDMODE_BIT:
			if (cursor > 0) {
				input_backward(1);
				input_delete(1);
			}
			break;
		case 'W'|VI_CMDMODE_BIT:
			vi_Word_motion(1);
			break;
		case 'w'|VI_CMDMODE_BIT:
			vi_word_motion(1);
			break;
		case 'E'|VI_CMDMODE_BIT:
			vi_End_motion();
			break;
		case 'e'|VI_CMDMODE_BIT:
			vi_end_motion();
			break;
		case 'B'|VI_CMDMODE_BIT:
			vi_Back_motion();
			break;
		case 'b'|VI_CMDMODE_BIT:
			vi_back_motion();
			break;
		case 'C'|VI_CMDMODE_BIT:
			vi_cmdmode = 0;
			/* fall through */
		case 'D'|VI_CMDMODE_BIT:
			goto clear_to_eol;

		case 'c'|VI_CMDMODE_BIT:
			vi_cmdmode = 0;
			/* fall through */
		case 'd'|VI_CMDMODE_BIT: {
			int nc, sc;

			ic = lineedit_read_key(read_key_buffer);
			if (errno) /* error */
				goto prepare_to_die;
			if (ic == ic_raw) { /* "cc", "dd" */
				input_backward(cursor);
				goto clear_to_eol;
				break;
			}

			sc = cursor;
			switch (ic) {
			case 'w':
			case 'W':
			case 'e':
			case 'E':
				switch (ic) {
				case 'w':   /* "dw", "cw" */
					vi_word_motion(vi_cmdmode);
					break;
				case 'W':   /* 'dW', 'cW' */
					vi_Word_motion(vi_cmdmode);
					break;
				case 'e':   /* 'de', 'ce' */
					vi_end_motion();
					input_forward();
					break;
				case 'E':   /* 'dE', 'cE' */
					vi_End_motion();
					input_forward();
					break;
				}
				nc = cursor;
				input_backward(cursor - sc);
				while (nc-- > cursor)
					input_delete(1);
				break;
			case 'b':  /* "db", "cb" */
			case 'B':  /* implemented as B */
				if (ic == 'b')
					vi_back_motion();
				else
					vi_Back_motion();
				while (sc-- > cursor)
					input_delete(1);
				break;
			case ' ':  /* "d ", "c " */
				input_delete(1);
				break;
			case '$':  /* "d$", "c$" */
 clear_to_eol:
				while (cursor < command_len)
					input_delete(1);
				break;
			}
			break;
		}
		case 'p'|VI_CMDMODE_BIT:
			input_forward();
			/* fallthrough */
		case 'P'|VI_CMDMODE_BIT:
			put();
			break;
		case 'r'|VI_CMDMODE_BIT:
//FIXME: unicode case?
			ic = lineedit_read_key(read_key_buffer);
			if (errno) /* error */
				goto prepare_to_die;
			if (ic < ' ' || ic > 255) {
				beep();
			} else {
				command_ps[cursor] = ic;
				bb_putchar(ic);
				bb_putchar('\b');
			}
			break;
		case '\x1b': /* ESC */
			if (state->flags & VI_MODE) {
				/* insert mode --> command mode */
				vi_cmdmode = 1;
				input_backward(1);
			}
			break;
#endif /* FEATURE_COMMAND_EDITING_VI */

#if MAX_HISTORY > 0
		case KEYCODE_UP:
			if (get_previous_history())
				goto rewrite_line;
			beep();
			break;
		case KEYCODE_DOWN:
			if (!get_next_history())
				break;
 rewrite_line:
			/* Rewrite the line with the selected history item */
			/* change command */
			command_len = load_string(state->history[state->cur_history] ?
					state->history[state->cur_history] : "", maxsize);
			/* redraw and go to eol (bol, in vi) */
			redraw(cmdedit_y, (state->flags & VI_MODE) ? 9999 : 0);
			break;
#endif
		case KEYCODE_RIGHT:
			input_forward();
			break;
		case KEYCODE_LEFT:
			input_backward(1);
			break;
		case KEYCODE_CTRL_LEFT:
			ctrl_left();
			break;
		case KEYCODE_CTRL_RIGHT:
			ctrl_right();
			break;
		case KEYCODE_HOME:
			input_backward(cursor);
			break;
		case KEYCODE_END:
			put_till_end_and_adv_cursor();
			break;

		default:
			if (initial_settings.c_cc[VINTR] != 0
			 && ic_raw == initial_settings.c_cc[VINTR]
			) {
				/* Ctrl-C (usually) - stop gathering input */
				goto_new_line();
				command_len = 0;
				break_out = -1; /* "do not append '\n'" */
				break;
			}
			if (initial_settings.c_cc[VEOF] != 0
			 && ic_raw == initial_settings.c_cc[VEOF]
			) {
				/* Ctrl-D (usually) - delete one character,
				 * or exit if len=0 and no chars to delete */
				if (command_len == 0) {
					errno = 0;
#if ENABLE_FEATURE_EDITING_VI
 prepare_to_die:
#endif
					break_out = command_len = -1;
					break;
				}
				input_delete(0);
				break;
			}
//			/* Control-V -- force insert of next char */
//			if (c == CTRL('V')) {
//				if (safe_read(STDIN_FILENO, &c, 1) < 1)
//					goto prepare_to_die;
//				if (c == 0) {
//					beep();
//					break;
//				}
//			}
			if (ic < ' '
			 || (!ENABLE_UNICODE_SUPPORT && ic >= 256)
			 || (ENABLE_UNICODE_SUPPORT && ic >= VI_CMDMODE_BIT)
			) {
				/* If VI_CMDMODE_BIT is set, ic is >= 256
				 * and vi mode ignores unexpected chars.
				 * Otherwise, we are here if ic is a
				 * control char or an unhandled ESC sequence,
				 * which is also ignored.
				 */
				break;
			}
			if ((int)command_len >= (maxsize - 2)) {
				/* Not enough space for the char and EOL */
				break;
			}

			command_len++;
			if (cursor == (command_len - 1)) {
				/* We are at the end, append */
				command_ps[cursor] = ic;
				command_ps[cursor + 1] = BB_NUL;
				put_cur_glyph_and_inc_cursor();
				if (unicode_bidi_isrtl(ic))
					input_backward(1);
			} else {
				/* In the middle, insert */
				int sc = cursor;

				memmove(command_ps + sc + 1, command_ps + sc,
					(command_len - sc) * sizeof(command_ps[0]));
				command_ps[sc] = ic;
				/* is right-to-left char, or neutral one (e.g. comma) was just added to rtl text? */
				if (!isrtl_str())
					sc++; /* no */
				put_till_end_and_adv_cursor();
				/* to prev x pos + 1 */
				input_backward(cursor - sc);
			}
			break;
		} /* switch (ic) */

		if (break_out)
			break;

#if ENABLE_FEATURE_TAB_COMPLETION
		if (ic_raw != '\t')
			lastWasTab = FALSE;
#endif
	} /* while (1) */

#if ENABLE_FEATURE_EDITING_ASK_TERMINAL
	if (S.sent_ESC_br6n) {
		/* "sleep 1; busybox ash" + hold [Enter] to trigger.
		 * We sent "ESC [ 6 n", but got '\n' first, and
		 * KEYCODE_CURSOR_POS response is now buffered from terminal.
		 * It's bad already and not much can be done with it
		 * (it _will_ be visible for the next process to read stdin),
		 * but without this delay it even shows up on the screen
		 * as garbage because we restore echo settings with tcsetattr
		 * before it comes in. UGLY!
		 */
		usleep(20*1000);
	}
#endif

/* Stop bug catching using "command_must_not_be_used" trick */
#undef command

#if ENABLE_UNICODE_SUPPORT
	command[0] = '\0';
	if (command_len > 0)
		command_len = save_string(command, maxsize - 1);
	free(command_ps);
#endif

	if (command_len > 0)
		remember_in_history(command);

	if (break_out > 0) {
		command[command_len++] = '\n';
		command[command_len] = '\0';
	}

#if ENABLE_FEATURE_TAB_COMPLETION
	free_tab_completion_data();
#endif

	/* restore initial_settings */
	tcsetattr_stdin_TCSANOW(&initial_settings);
	/* restore SIGWINCH handler */
	signal(SIGWINCH, previous_SIGWINCH_handler);
	fflush_all();

	len = command_len;
	DEINIT_S();

	return len; /* can't return command_len, DEINIT_S() destroys it */
}

#else

#undef read_line_input
int FAST_FUNC read_line_input(const char* prompt, char* command, int maxsize)
{
	fputs(prompt, stdout);
	fflush_all();
	fgets(command, maxsize, stdin);
	return strlen(command);
}

#endif  /* FEATURE_EDITING */


/*
 * Testing
 */

#ifdef TEST

#include <locale.h>

const char *applet_name = "debug stuff usage";

int main(int argc, char **argv)
{
	char buff[MAX_LINELEN];
	char *prompt =
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
		"\\[\\033[32;1m\\]\\u@\\[\\x1b[33;1m\\]\\h:"
		"\\[\\033[34;1m\\]\\w\\[\\033[35;1m\\] "
		"\\!\\[\\e[36;1m\\]\\$ \\[\\E[0m\\]";
#else
		"% ";
#endif

	while (1) {
		int l;
		l = read_line_input(prompt, buff);
		if (l <= 0 || buff[l-1] != '\n')
			break;
		buff[l-1] = 0;
		printf("*** read_line_input() returned line =%s=\n", buff);
	}
	printf("*** read_line_input() detect ^D\n");
	return 0;
}

#endif  /* TEST */
