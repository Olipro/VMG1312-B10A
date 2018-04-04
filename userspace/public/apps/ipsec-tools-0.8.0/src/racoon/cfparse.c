
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 5 "cfparse.y"

/*
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002 and 2003 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include PATH_IPSEC_H

#ifdef ENABLE_HYBRID
#include <arpa/inet.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "str2val.h"
#include "genlist.h"
#include "debug.h"

#include "admin.h"
#include "privsep.h"
#include "cfparse_proto.h"
#include "cftoken_proto.h"
#include "algorithm.h"
#include "localconf.h"
#include "policy.h"
#include "sainfo.h"
#include "oakley.h"
#include "pfkey.h"
#include "remoteconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "handler.h"
#include "isakmp.h"
#include "nattraversal.h"
#include "isakmp_frag.h"
#ifdef ENABLE_HYBRID
#include "resolv.h"
#include "isakmp_unity.h"
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#endif
#include "ipsec_doi.h"
#include "strnames.h"
#include "gcmalloc.h"
#ifdef HAVE_GSSAPI
#include "gssapi.h"
#endif
#include "vendorid.h"
#include "rsalist.h"
#include "crypto_openssl.h"

struct secprotospec {
	int prop_no;
	int trns_no;
	int strength;		/* for isakmp/ipsec */
	int encklen;		/* for isakmp/ipsec */
	time_t lifetime;	/* for isakmp */
	int lifebyte;		/* for isakmp */
	int proto_id;		/* for ipsec (isakmp?) */
	int ipsec_level;	/* for ipsec */
	int encmode;		/* for ipsec */
	int vendorid;		/* for isakmp */
	char *gssid;
	struct sockaddr *remote;
	int algclass[MAXALGCLASS];

	struct secprotospec *next;	/* the tail is the most prefiered. */
	struct secprotospec *prev;
};

static int num2dhgroup[] = {
	0,
	OAKLEY_ATTR_GRP_DESC_MODP768,
	OAKLEY_ATTR_GRP_DESC_MODP1024,
	OAKLEY_ATTR_GRP_DESC_EC2N155,
	OAKLEY_ATTR_GRP_DESC_EC2N185,
	OAKLEY_ATTR_GRP_DESC_MODP1536,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	OAKLEY_ATTR_GRP_DESC_MODP2048,
	OAKLEY_ATTR_GRP_DESC_MODP3072,
	OAKLEY_ATTR_GRP_DESC_MODP4096,
	OAKLEY_ATTR_GRP_DESC_MODP6144,
	OAKLEY_ATTR_GRP_DESC_MODP8192
};

static struct remoteconf *cur_rmconf;
static int tmpalgtype[MAXALGCLASS];
static struct sainfo *cur_sainfo;
static int cur_algclass;
static int oldloglevel = LLV_BASE;

static struct secprotospec *newspspec __P((void));
static void insspspec __P((struct remoteconf *, struct secprotospec *));
void dupspspec_list __P((struct remoteconf *dst, struct remoteconf *src));
void flushspspec __P((struct remoteconf *));
static void adminsock_conf __P((vchar_t *, vchar_t *, vchar_t *, int));

static int set_isakmp_proposal __P((struct remoteconf *));
static void clean_tmpalgtype __P((void));
static int expand_isakmpspec __P((int, int, int *,
	int, int, time_t, int, int, int, char *, struct remoteconf *));

void freeetypes (struct etypes **etypes);

static int load_x509(const char *file, char **filenameptr,
		     vchar_t **certptr)
{
	char path[PATH_MAX];

	getpathname(path, sizeof(path), LC_PATHTYPE_CERT, file);
	*certptr = eay_get_x509cert(path);
	if (*certptr == NULL)
		return -1;

	*filenameptr = racoon_strdup(file);
	STRDUP_FATAL(*filenameptr);

	return 0;
}



/* Line 189 of yacc.c  */
#line 246 "cfparse.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     PRIVSEP = 258,
     USER = 259,
     GROUP = 260,
     CHROOT = 261,
     PATH = 262,
     PATHTYPE = 263,
     INCLUDE = 264,
     PFKEY_BUFFER = 265,
     LOGGING = 266,
     LOGLEV = 267,
     PADDING = 268,
     PAD_RANDOMIZE = 269,
     PAD_RANDOMIZELEN = 270,
     PAD_MAXLEN = 271,
     PAD_STRICT = 272,
     PAD_EXCLTAIL = 273,
     LISTEN = 274,
     X_ISAKMP = 275,
     X_ISAKMP_NATT = 276,
     X_ADMIN = 277,
     STRICT_ADDRESS = 278,
     ADMINSOCK = 279,
     DISABLED = 280,
     LDAPCFG = 281,
     LDAP_HOST = 282,
     LDAP_PORT = 283,
     LDAP_PVER = 284,
     LDAP_BASE = 285,
     LDAP_BIND_DN = 286,
     LDAP_BIND_PW = 287,
     LDAP_SUBTREE = 288,
     LDAP_ATTR_USER = 289,
     LDAP_ATTR_ADDR = 290,
     LDAP_ATTR_MASK = 291,
     LDAP_ATTR_GROUP = 292,
     LDAP_ATTR_MEMBER = 293,
     RADCFG = 294,
     RAD_AUTH = 295,
     RAD_ACCT = 296,
     RAD_TIMEOUT = 297,
     RAD_RETRIES = 298,
     MODECFG = 299,
     CFG_NET4 = 300,
     CFG_MASK4 = 301,
     CFG_DNS4 = 302,
     CFG_NBNS4 = 303,
     CFG_DEFAULT_DOMAIN = 304,
     CFG_AUTH_SOURCE = 305,
     CFG_AUTH_GROUPS = 306,
     CFG_SYSTEM = 307,
     CFG_RADIUS = 308,
     CFG_PAM = 309,
     CFG_LDAP = 310,
     CFG_LOCAL = 311,
     CFG_NONE = 312,
     CFG_GROUP_SOURCE = 313,
     CFG_ACCOUNTING = 314,
     CFG_CONF_SOURCE = 315,
     CFG_MOTD = 316,
     CFG_POOL_SIZE = 317,
     CFG_AUTH_THROTTLE = 318,
     CFG_SPLIT_NETWORK = 319,
     CFG_SPLIT_LOCAL = 320,
     CFG_SPLIT_INCLUDE = 321,
     CFG_SPLIT_DNS = 322,
     CFG_PFS_GROUP = 323,
     CFG_SAVE_PASSWD = 324,
     RETRY = 325,
     RETRY_COUNTER = 326,
     RETRY_INTERVAL = 327,
     RETRY_PERSEND = 328,
     RETRY_PHASE1 = 329,
     RETRY_PHASE2 = 330,
     NATT_KA = 331,
     ALGORITHM_CLASS = 332,
     ALGORITHMTYPE = 333,
     STRENGTHTYPE = 334,
     SAINFO = 335,
     FROM = 336,
     REMOTE = 337,
     ANONYMOUS = 338,
     CLIENTADDR = 339,
     INHERIT = 340,
     REMOTE_ADDRESS = 341,
     EXCHANGE_MODE = 342,
     EXCHANGETYPE = 343,
     DOI = 344,
     DOITYPE = 345,
     SITUATION = 346,
     SITUATIONTYPE = 347,
     CERTIFICATE_TYPE = 348,
     CERTTYPE = 349,
     PEERS_CERTFILE = 350,
     CA_TYPE = 351,
     VERIFY_CERT = 352,
     SEND_CERT = 353,
     SEND_CR = 354,
     MATCH_EMPTY_CR = 355,
     IDENTIFIERTYPE = 356,
     IDENTIFIERQUAL = 357,
     MY_IDENTIFIER = 358,
     PEERS_IDENTIFIER = 359,
     VERIFY_IDENTIFIER = 360,
     DNSSEC = 361,
     CERT_X509 = 362,
     CERT_PLAINRSA = 363,
     NONCE_SIZE = 364,
     DH_GROUP = 365,
     KEEPALIVE = 366,
     PASSIVE = 367,
     INITIAL_CONTACT = 368,
     NAT_TRAVERSAL = 369,
     REMOTE_FORCE_LEVEL = 370,
     PROPOSAL_CHECK = 371,
     PROPOSAL_CHECK_LEVEL = 372,
     GENERATE_POLICY = 373,
     GENERATE_LEVEL = 374,
     SUPPORT_PROXY = 375,
     PROPOSAL = 376,
     EXEC_PATH = 377,
     EXEC_COMMAND = 378,
     EXEC_SUCCESS = 379,
     EXEC_FAILURE = 380,
     GSS_ID = 381,
     GSS_ID_ENC = 382,
     GSS_ID_ENCTYPE = 383,
     COMPLEX_BUNDLE = 384,
     DPD = 385,
     DPD_DELAY = 386,
     DPD_RETRY = 387,
     DPD_MAXFAIL = 388,
     PH1ID = 389,
     XAUTH_LOGIN = 390,
     WEAK_PHASE1_CHECK = 391,
     REKEY = 392,
     PREFIX = 393,
     PORT = 394,
     PORTANY = 395,
     UL_PROTO = 396,
     ANY = 397,
     IKE_FRAG = 398,
     ESP_FRAG = 399,
     MODE_CFG = 400,
     PFS_GROUP = 401,
     LIFETIME = 402,
     LIFETYPE_TIME = 403,
     LIFETYPE_BYTE = 404,
     STRENGTH = 405,
     REMOTEID = 406,
     SCRIPT = 407,
     PHASE1_UP = 408,
     PHASE1_DOWN = 409,
     PHASE1_DEAD = 410,
     NUMBER = 411,
     SWITCH = 412,
     BOOLEAN = 413,
     HEXSTRING = 414,
     QUOTEDSTRING = 415,
     ADDRSTRING = 416,
     ADDRRANGE = 417,
     UNITTYPE_BYTE = 418,
     UNITTYPE_KBYTES = 419,
     UNITTYPE_MBYTES = 420,
     UNITTYPE_TBYTES = 421,
     UNITTYPE_SEC = 422,
     UNITTYPE_MIN = 423,
     UNITTYPE_HOUR = 424,
     EOS = 425,
     BOC = 426,
     EOC = 427,
     COMMA = 428,
     REMOTE_ACCESS = 429,
     DYNAMIC_SITE = 430,
     SA_ASSOC = 431,
     USE_NATT_ADDR = 432
   };
#endif
/* Tokens.  */
#define PRIVSEP 258
#define USER 259
#define GROUP 260
#define CHROOT 261
#define PATH 262
#define PATHTYPE 263
#define INCLUDE 264
#define PFKEY_BUFFER 265
#define LOGGING 266
#define LOGLEV 267
#define PADDING 268
#define PAD_RANDOMIZE 269
#define PAD_RANDOMIZELEN 270
#define PAD_MAXLEN 271
#define PAD_STRICT 272
#define PAD_EXCLTAIL 273
#define LISTEN 274
#define X_ISAKMP 275
#define X_ISAKMP_NATT 276
#define X_ADMIN 277
#define STRICT_ADDRESS 278
#define ADMINSOCK 279
#define DISABLED 280
#define LDAPCFG 281
#define LDAP_HOST 282
#define LDAP_PORT 283
#define LDAP_PVER 284
#define LDAP_BASE 285
#define LDAP_BIND_DN 286
#define LDAP_BIND_PW 287
#define LDAP_SUBTREE 288
#define LDAP_ATTR_USER 289
#define LDAP_ATTR_ADDR 290
#define LDAP_ATTR_MASK 291
#define LDAP_ATTR_GROUP 292
#define LDAP_ATTR_MEMBER 293
#define RADCFG 294
#define RAD_AUTH 295
#define RAD_ACCT 296
#define RAD_TIMEOUT 297
#define RAD_RETRIES 298
#define MODECFG 299
#define CFG_NET4 300
#define CFG_MASK4 301
#define CFG_DNS4 302
#define CFG_NBNS4 303
#define CFG_DEFAULT_DOMAIN 304
#define CFG_AUTH_SOURCE 305
#define CFG_AUTH_GROUPS 306
#define CFG_SYSTEM 307
#define CFG_RADIUS 308
#define CFG_PAM 309
#define CFG_LDAP 310
#define CFG_LOCAL 311
#define CFG_NONE 312
#define CFG_GROUP_SOURCE 313
#define CFG_ACCOUNTING 314
#define CFG_CONF_SOURCE 315
#define CFG_MOTD 316
#define CFG_POOL_SIZE 317
#define CFG_AUTH_THROTTLE 318
#define CFG_SPLIT_NETWORK 319
#define CFG_SPLIT_LOCAL 320
#define CFG_SPLIT_INCLUDE 321
#define CFG_SPLIT_DNS 322
#define CFG_PFS_GROUP 323
#define CFG_SAVE_PASSWD 324
#define RETRY 325
#define RETRY_COUNTER 326
#define RETRY_INTERVAL 327
#define RETRY_PERSEND 328
#define RETRY_PHASE1 329
#define RETRY_PHASE2 330
#define NATT_KA 331
#define ALGORITHM_CLASS 332
#define ALGORITHMTYPE 333
#define STRENGTHTYPE 334
#define SAINFO 335
#define FROM 336
#define REMOTE 337
#define ANONYMOUS 338
#define CLIENTADDR 339
#define INHERIT 340
#define REMOTE_ADDRESS 341
#define EXCHANGE_MODE 342
#define EXCHANGETYPE 343
#define DOI 344
#define DOITYPE 345
#define SITUATION 346
#define SITUATIONTYPE 347
#define CERTIFICATE_TYPE 348
#define CERTTYPE 349
#define PEERS_CERTFILE 350
#define CA_TYPE 351
#define VERIFY_CERT 352
#define SEND_CERT 353
#define SEND_CR 354
#define MATCH_EMPTY_CR 355
#define IDENTIFIERTYPE 356
#define IDENTIFIERQUAL 357
#define MY_IDENTIFIER 358
#define PEERS_IDENTIFIER 359
#define VERIFY_IDENTIFIER 360
#define DNSSEC 361
#define CERT_X509 362
#define CERT_PLAINRSA 363
#define NONCE_SIZE 364
#define DH_GROUP 365
#define KEEPALIVE 366
#define PASSIVE 367
#define INITIAL_CONTACT 368
#define NAT_TRAVERSAL 369
#define REMOTE_FORCE_LEVEL 370
#define PROPOSAL_CHECK 371
#define PROPOSAL_CHECK_LEVEL 372
#define GENERATE_POLICY 373
#define GENERATE_LEVEL 374
#define SUPPORT_PROXY 375
#define PROPOSAL 376
#define EXEC_PATH 377
#define EXEC_COMMAND 378
#define EXEC_SUCCESS 379
#define EXEC_FAILURE 380
#define GSS_ID 381
#define GSS_ID_ENC 382
#define GSS_ID_ENCTYPE 383
#define COMPLEX_BUNDLE 384
#define DPD 385
#define DPD_DELAY 386
#define DPD_RETRY 387
#define DPD_MAXFAIL 388
#define PH1ID 389
#define XAUTH_LOGIN 390
#define WEAK_PHASE1_CHECK 391
#define REKEY 392
#define PREFIX 393
#define PORT 394
#define PORTANY 395
#define UL_PROTO 396
#define ANY 397
#define IKE_FRAG 398
#define ESP_FRAG 399
#define MODE_CFG 400
#define PFS_GROUP 401
#define LIFETIME 402
#define LIFETYPE_TIME 403
#define LIFETYPE_BYTE 404
#define STRENGTH 405
#define REMOTEID 406
#define SCRIPT 407
#define PHASE1_UP 408
#define PHASE1_DOWN 409
#define PHASE1_DEAD 410
#define NUMBER 411
#define SWITCH 412
#define BOOLEAN 413
#define HEXSTRING 414
#define QUOTEDSTRING 415
#define ADDRSTRING 416
#define ADDRRANGE 417
#define UNITTYPE_BYTE 418
#define UNITTYPE_KBYTES 419
#define UNITTYPE_MBYTES 420
#define UNITTYPE_TBYTES 421
#define UNITTYPE_SEC 422
#define UNITTYPE_MIN 423
#define UNITTYPE_HOUR 424
#define EOS 425
#define BOC 426
#define EOC 427
#define COMMA 428
#define REMOTE_ACCESS 429
#define DYNAMIC_SITE 430
#define SA_ASSOC 431
#define USE_NATT_ADDR 432




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 177 "cfparse.y"

	unsigned long num;
	vchar_t *val;
	struct remoteconf *rmconf;
	struct sockaddr *saddr;
	struct sainfoalg *alg;



/* Line 214 of yacc.c  */
#line 646 "cfparse.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 658 "cfparse.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   553

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  178
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  207
/* YYNRULES -- Number of rules.  */
#define YYNRULES  387
/* YYNRULES -- Number of states.  */
#define YYNSTATES  705

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   432

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    27,    29,    31,    33,    35,    37,
      42,    43,    46,    47,    52,    53,    58,    59,    64,    65,
      70,    71,    76,    77,    83,    84,    89,    93,    97,   101,
     105,   107,   112,   113,   116,   117,   122,   123,   128,   129,
     134,   135,   140,   141,   146,   151,   152,   155,   156,   161,
     162,   167,   168,   176,   177,   182,   183,   188,   189,   193,
     196,   197,   199,   200,   206,   207,   210,   211,   217,   218,
     225,   226,   232,   233,   240,   241,   246,   247,   252,   253,
     259,   260,   263,   264,   269,   270,   275,   276,   281,   282,
     287,   288,   293,   294,   299,   300,   305,   306,   311,   312,
     317,   318,   323,   324,   329,   330,   335,   340,   341,   344,
     345,   350,   351,   356,   360,   364,   365,   371,   372,   378,
     379,   384,   385,   390,   391,   396,   397,   402,   403,   408,
     409,   414,   415,   420,   421,   426,   427,   432,   433,   438,
     439,   444,   445,   450,   451,   456,   457,   462,   463,   468,
     469,   474,   475,   480,   481,   486,   487,   492,   493,   498,
     499,   504,   506,   510,   512,   514,   518,   520,   522,   526,
     529,   531,   535,   537,   539,   543,   545,   550,   551,   554,
     555,   560,   561,   567,   568,   573,   574,   580,   581,   587,
     588,   594,   595,   596,   605,   607,   610,   613,   616,   619,
     622,   628,   635,   638,   639,   643,   646,   647,   650,   651,
     656,   657,   662,   663,   670,   671,   678,   679,   684,   686,
     687,   692,   695,   696,   698,   699,   701,   703,   705,   707,
     709,   710,   712,   713,   720,   721,   726,   727,   734,   735,
     740,   744,   747,   749,   750,   753,   754,   759,   760,   765,
     766,   771,   772,   777,   780,   781,   786,   787,   793,   794,
     800,   801,   806,   807,   813,   814,   819,   820,   825,   826,
     831,   832,   837,   838,   844,   845,   852,   853,   858,   859,
     865,   866,   873,   874,   879,   880,   885,   886,   891,   892,
     897,   898,   903,   904,   909,   910,   915,   916,   921,   922,
     927,   928,   933,   934,   939,   940,   946,   947,   953,   954,
     960,   961,   966,   967,   972,   973,   978,   979,   984,   985,
     990,   991,   996,   997,  1002,  1003,  1008,  1009,  1014,  1015,
    1020,  1021,  1026,  1027,  1032,  1033,  1038,  1039,  1044,  1045,
    1050,  1051,  1058,  1059,  1064,  1065,  1072,  1073,  1079,  1080,
    1083,  1084,  1090,  1091,  1096,  1098,  1100,  1101,  1103,  1105,
    1106,  1109,  1110,  1117,  1118,  1125,  1126,  1131,  1132,  1137,
    1138,  1144,  1146,  1148,  1150,  1152,  1154,  1156
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     179,     0,    -1,    -1,   179,   180,    -1,   181,    -1,   189,
      -1,   193,    -1,   194,    -1,   195,    -1,   196,    -1,   198,
      -1,   206,    -1,   227,    -1,   217,    -1,   243,    -1,   281,
      -1,   290,    -1,   310,    -1,   191,    -1,     3,   171,   182,
     172,    -1,    -1,   182,   183,    -1,    -1,     4,   160,   184,
     170,    -1,    -1,     4,   156,   185,   170,    -1,    -1,     5,
     160,   186,   170,    -1,    -1,     5,   156,   187,   170,    -1,
      -1,     6,   160,   188,   170,    -1,    -1,     7,     8,   160,
     190,   170,    -1,    -1,   129,   157,   192,   170,    -1,     9,
     160,   170,    -1,    10,   156,   170,    -1,   127,   128,   170,
      -1,    11,   197,   170,    -1,    12,    -1,    13,   171,   199,
     172,    -1,    -1,   199,   200,    -1,    -1,    14,   157,   201,
     170,    -1,    -1,    15,   157,   202,   170,    -1,    -1,    16,
     156,   203,   170,    -1,    -1,    17,   157,   204,   170,    -1,
      -1,    18,   157,   205,   170,    -1,    19,   171,   207,   172,
      -1,    -1,   207,   208,    -1,    -1,    20,   215,   209,   170,
      -1,    -1,    21,   215,   210,   170,    -1,    -1,    24,   160,
     160,   160,   156,   211,   170,    -1,    -1,    24,   160,   212,
     170,    -1,    -1,    24,    25,   213,   170,    -1,    -1,    23,
     214,   170,    -1,   161,   216,    -1,    -1,   139,    -1,    -1,
      39,   218,   171,   219,   172,    -1,    -1,   219,   220,    -1,
      -1,    40,   160,   160,   221,   170,    -1,    -1,    40,   160,
     156,   160,   222,   170,    -1,    -1,    41,   160,   160,   223,
     170,    -1,    -1,    41,   160,   156,   160,   224,   170,    -1,
      -1,    42,   156,   225,   170,    -1,    -1,    43,   156,   226,
     170,    -1,    -1,    26,   228,   171,   229,   172,    -1,    -1,
     229,   230,    -1,    -1,    29,   156,   231,   170,    -1,    -1,
      27,   160,   232,   170,    -1,    -1,    28,   156,   233,   170,
      -1,    -1,    30,   160,   234,   170,    -1,    -1,    33,   157,
     235,   170,    -1,    -1,    31,   160,   236,   170,    -1,    -1,
      32,   160,   237,   170,    -1,    -1,    34,   160,   238,   170,
      -1,    -1,    35,   160,   239,   170,    -1,    -1,    36,   160,
     240,   170,    -1,    -1,    37,   160,   241,   170,    -1,    -1,
      38,   160,   242,   170,    -1,    44,   171,   244,   172,    -1,
      -1,   244,   245,    -1,    -1,    45,   161,   246,   170,    -1,
      -1,    46,   161,   247,   170,    -1,    47,   271,   170,    -1,
      48,   273,   170,    -1,    -1,    64,    65,   275,   248,   170,
      -1,    -1,    64,    66,   275,   249,   170,    -1,    -1,    67,
     279,   250,   170,    -1,    -1,    49,   160,   251,   170,    -1,
      -1,    50,    52,   252,   170,    -1,    -1,    50,    53,   253,
     170,    -1,    -1,    50,    54,   254,   170,    -1,    -1,    50,
      55,   255,   170,    -1,    -1,    51,   277,   256,   170,    -1,
      -1,    58,    52,   257,   170,    -1,    -1,    58,    55,   258,
     170,    -1,    -1,    59,    57,   259,   170,    -1,    -1,    59,
      52,   260,   170,    -1,    -1,    59,    53,   261,   170,    -1,
      -1,    59,    54,   262,   170,    -1,    -1,    62,   156,   263,
     170,    -1,    -1,    68,   156,   264,   170,    -1,    -1,    69,
     157,   265,   170,    -1,    -1,    63,   156,   266,   170,    -1,
      -1,    60,    56,   267,   170,    -1,    -1,    60,    53,   268,
     170,    -1,    -1,    60,    55,   269,   170,    -1,    -1,    61,
     160,   270,   170,    -1,   272,    -1,   272,   173,   271,    -1,
     161,    -1,   274,    -1,   274,   173,   273,    -1,   161,    -1,
     276,    -1,   275,   173,   276,    -1,   161,   138,    -1,   278,
      -1,   278,   173,   277,    -1,   160,    -1,   280,    -1,   280,
     173,   279,    -1,   160,    -1,    70,   171,   282,   172,    -1,
      -1,   282,   283,    -1,    -1,    71,   156,   284,   170,    -1,
      -1,    72,   156,   383,   285,   170,    -1,    -1,    73,   156,
     286,   170,    -1,    -1,    74,   156,   383,   287,   170,    -1,
      -1,    75,   156,   383,   288,   170,    -1,    -1,    76,   156,
     383,   289,   170,    -1,    -1,    -1,    80,   291,   293,   295,
     171,   296,   292,   172,    -1,    83,    -1,    83,    84,    -1,
      83,   294,    -1,   294,    83,    -1,   294,    84,    -1,   294,
     294,    -1,   101,   161,   306,   307,   308,    -1,   101,   161,
     162,   306,   307,   308,    -1,   101,   160,    -1,    -1,    81,
     101,   375,    -1,     5,   160,    -1,    -1,   296,   297,    -1,
      -1,   146,   374,   298,   170,    -1,    -1,   151,   156,   299,
     170,    -1,    -1,   147,   148,   156,   383,   300,   170,    -1,
      -1,   147,   149,   156,   384,   301,   170,    -1,    -1,    77,
     302,   303,   170,    -1,   305,    -1,    -1,   305,   304,   173,
     303,    -1,    78,   309,    -1,    -1,   138,    -1,    -1,   139,
      -1,   140,    -1,   156,    -1,   141,    -1,   142,    -1,    -1,
     156,    -1,    -1,    82,   160,    85,   160,   311,   315,    -1,
      -1,    82,   160,   312,   315,    -1,    -1,    82,   316,    85,
     316,   313,   315,    -1,    -1,    82,   316,   314,   315,    -1,
     171,   317,   172,    -1,    83,   216,    -1,   215,    -1,    -1,
     317,   318,    -1,    -1,    86,   215,   319,   170,    -1,    -1,
      87,   320,   370,   170,    -1,    -1,    89,    90,   321,   170,
      -1,    -1,    91,    92,   322,   170,    -1,    93,   371,    -1,
      -1,    95,   160,   323,   170,    -1,    -1,    95,   107,   160,
     324,   170,    -1,    -1,    95,   108,   160,   325,   170,    -1,
      -1,    95,   106,   326,   170,    -1,    -1,    96,   107,   160,
     327,   170,    -1,    -1,    97,   157,   328,   170,    -1,    -1,
      98,   157,   329,   170,    -1,    -1,    99,   157,   330,   170,
      -1,    -1,   100,   157,   331,   170,    -1,    -1,   103,   101,
     375,   332,   170,    -1,    -1,   103,   101,   102,   375,   333,
     170,    -1,    -1,   135,   375,   334,   170,    -1,    -1,   104,
     101,   375,   335,   170,    -1,    -1,   104,   101,   102,   375,
     336,   170,    -1,    -1,   105,   157,   337,   170,    -1,    -1,
     109,   156,   338,   170,    -1,    -1,   110,   339,   374,   170,
      -1,    -1,   112,   157,   340,   170,    -1,    -1,   174,   157,
     341,   170,    -1,    -1,   175,   157,   342,   170,    -1,    -1,
     176,   157,   343,   170,    -1,    -1,   177,   157,   344,   170,
      -1,    -1,   143,   157,   345,   170,    -1,    -1,   143,   115,
     346,   170,    -1,    -1,   144,   156,   347,   170,    -1,    -1,
     152,   160,   153,   348,   170,    -1,    -1,   152,   160,   154,
     349,   170,    -1,    -1,   152,   160,   155,   350,   170,    -1,
      -1,   145,   157,   351,   170,    -1,    -1,   136,   157,   352,
     170,    -1,    -1,   118,   157,   353,   170,    -1,    -1,   118,
     119,   354,   170,    -1,    -1,   120,   157,   355,   170,    -1,
      -1,   113,   157,   356,   170,    -1,    -1,   114,   157,   357,
     170,    -1,    -1,   114,   115,   358,   170,    -1,    -1,   130,
     157,   359,   170,    -1,    -1,   131,   156,   360,   170,    -1,
      -1,   132,   156,   361,   170,    -1,    -1,   133,   156,   362,
     170,    -1,    -1,   137,   157,   363,   170,    -1,    -1,   137,
     115,   364,   170,    -1,    -1,   134,   156,   365,   170,    -1,
      -1,   147,   148,   156,   383,   366,   170,    -1,    -1,   116,
     117,   367,   170,    -1,    -1,   147,   149,   156,   384,   368,
     170,    -1,    -1,   121,   369,   171,   376,   172,    -1,    -1,
     370,    88,    -1,    -1,   107,   160,   160,   372,   170,    -1,
      -1,   108,   160,   373,   170,    -1,    78,    -1,   156,    -1,
      -1,   161,    -1,   160,    -1,    -1,   376,   377,    -1,    -1,
     147,   148,   156,   383,   378,   170,    -1,    -1,   147,   149,
     156,   384,   379,   170,    -1,    -1,   110,   374,   380,   170,
      -1,    -1,   126,   160,   381,   170,    -1,    -1,    77,    78,
     309,   382,   170,    -1,   167,    -1,   168,    -1,   169,    -1,
     163,    -1,   164,    -1,   165,    -1,   166,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   270,   270,   272,   275,   276,   277,   278,   279,   280,
     281,   282,   283,   284,   285,   286,   287,   288,   289,   294,
     296,   298,   302,   301,   312,   312,   314,   313,   324,   324,
     325,   325,   331,   330,   351,   351,   356,   370,   377,   389,
     392,   406,   408,   410,   413,   413,   414,   414,   415,   415,
     416,   416,   417,   417,   422,   424,   426,   430,   429,   436,
     435,   447,   446,   456,   455,   465,   464,   473,   473,   476,
     488,   489,   494,   494,   511,   513,   517,   516,   535,   534,
     553,   552,   571,   570,   589,   588,   598,   597,   610,   610,
     621,   623,   627,   626,   638,   637,   649,   648,   658,   657,
     669,   668,   678,   677,   689,   688,   700,   699,   711,   710,
     722,   721,   733,   732,   744,   743,   758,   760,   762,   766,
     765,   777,   776,   787,   789,   792,   791,   801,   800,   810,
     809,   817,   816,   829,   828,   838,   837,   851,   850,   864,
     863,   877,   876,   884,   883,   893,   892,   906,   905,   915,
     914,   924,   923,   937,   936,   950,   949,   960,   959,   969,
     968,   978,   977,   987,   986,   996,   995,  1009,  1008,  1022,
    1021,  1035,  1036,  1039,  1056,  1057,  1060,  1077,  1078,  1081,
    1104,  1105,  1108,  1142,  1143,  1146,  1183,  1185,  1187,  1191,
    1190,  1196,  1195,  1201,  1200,  1206,  1205,  1211,  1210,  1216,
    1215,  1232,  1240,  1231,  1279,  1284,  1289,  1294,  1299,  1304,
    1311,  1360,  1425,  1454,  1457,  1482,  1495,  1497,  1501,  1500,
    1506,  1505,  1511,  1510,  1516,  1515,  1527,  1527,  1534,  1539,
    1538,  1545,  1601,  1602,  1605,  1606,  1607,  1610,  1611,  1612,
    1615,  1616,  1622,  1621,  1652,  1651,  1672,  1671,  1695,  1694,
    1711,  1788,  1794,  1803,  1805,  1809,  1808,  1818,  1817,  1822,
    1822,  1823,  1823,  1824,  1826,  1825,  1846,  1845,  1863,  1862,
    1893,  1892,  1907,  1906,  1923,  1923,  1924,  1924,  1925,  1925,
    1926,  1926,  1928,  1927,  1937,  1936,  1946,  1945,  1963,  1962,
    1980,  1979,  1996,  1996,  1997,  1997,  1999,  1998,  2004,  2004,
    2005,  2005,  2006,  2006,  2007,  2007,  2008,  2008,  2009,  2009,
    2010,  2010,  2011,  2011,  2021,  2021,  2028,  2028,  2035,  2035,
    2042,  2042,  2043,  2043,  2046,  2046,  2047,  2047,  2048,  2048,
    2049,  2049,  2051,  2050,  2062,  2061,  2073,  2072,  2081,  2080,
    2090,  2089,  2099,  2098,  2107,  2107,  2108,  2108,  2110,  2109,
    2115,  2114,  2119,  2119,  2121,  2120,  2135,  2134,  2145,  2147,
    2171,  2170,  2192,  2191,  2225,  2233,  2245,  2246,  2247,  2249,
    2251,  2255,  2254,  2260,  2259,  2272,  2271,  2277,  2276,  2290,
    2289,  2387,  2388,  2389,  2392,  2393,  2394,  2395
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PRIVSEP", "USER", "GROUP", "CHROOT",
  "PATH", "PATHTYPE", "INCLUDE", "PFKEY_BUFFER", "LOGGING", "LOGLEV",
  "PADDING", "PAD_RANDOMIZE", "PAD_RANDOMIZELEN", "PAD_MAXLEN",
  "PAD_STRICT", "PAD_EXCLTAIL", "LISTEN", "X_ISAKMP", "X_ISAKMP_NATT",
  "X_ADMIN", "STRICT_ADDRESS", "ADMINSOCK", "DISABLED", "LDAPCFG",
  "LDAP_HOST", "LDAP_PORT", "LDAP_PVER", "LDAP_BASE", "LDAP_BIND_DN",
  "LDAP_BIND_PW", "LDAP_SUBTREE", "LDAP_ATTR_USER", "LDAP_ATTR_ADDR",
  "LDAP_ATTR_MASK", "LDAP_ATTR_GROUP", "LDAP_ATTR_MEMBER", "RADCFG",
  "RAD_AUTH", "RAD_ACCT", "RAD_TIMEOUT", "RAD_RETRIES", "MODECFG",
  "CFG_NET4", "CFG_MASK4", "CFG_DNS4", "CFG_NBNS4", "CFG_DEFAULT_DOMAIN",
  "CFG_AUTH_SOURCE", "CFG_AUTH_GROUPS", "CFG_SYSTEM", "CFG_RADIUS",
  "CFG_PAM", "CFG_LDAP", "CFG_LOCAL", "CFG_NONE", "CFG_GROUP_SOURCE",
  "CFG_ACCOUNTING", "CFG_CONF_SOURCE", "CFG_MOTD", "CFG_POOL_SIZE",
  "CFG_AUTH_THROTTLE", "CFG_SPLIT_NETWORK", "CFG_SPLIT_LOCAL",
  "CFG_SPLIT_INCLUDE", "CFG_SPLIT_DNS", "CFG_PFS_GROUP", "CFG_SAVE_PASSWD",
  "RETRY", "RETRY_COUNTER", "RETRY_INTERVAL", "RETRY_PERSEND",
  "RETRY_PHASE1", "RETRY_PHASE2", "NATT_KA", "ALGORITHM_CLASS",
  "ALGORITHMTYPE", "STRENGTHTYPE", "SAINFO", "FROM", "REMOTE", "ANONYMOUS",
  "CLIENTADDR", "INHERIT", "REMOTE_ADDRESS", "EXCHANGE_MODE",
  "EXCHANGETYPE", "DOI", "DOITYPE", "SITUATION", "SITUATIONTYPE",
  "CERTIFICATE_TYPE", "CERTTYPE", "PEERS_CERTFILE", "CA_TYPE",
  "VERIFY_CERT", "SEND_CERT", "SEND_CR", "MATCH_EMPTY_CR",
  "IDENTIFIERTYPE", "IDENTIFIERQUAL", "MY_IDENTIFIER", "PEERS_IDENTIFIER",
  "VERIFY_IDENTIFIER", "DNSSEC", "CERT_X509", "CERT_PLAINRSA",
  "NONCE_SIZE", "DH_GROUP", "KEEPALIVE", "PASSIVE", "INITIAL_CONTACT",
  "NAT_TRAVERSAL", "REMOTE_FORCE_LEVEL", "PROPOSAL_CHECK",
  "PROPOSAL_CHECK_LEVEL", "GENERATE_POLICY", "GENERATE_LEVEL",
  "SUPPORT_PROXY", "PROPOSAL", "EXEC_PATH", "EXEC_COMMAND", "EXEC_SUCCESS",
  "EXEC_FAILURE", "GSS_ID", "GSS_ID_ENC", "GSS_ID_ENCTYPE",
  "COMPLEX_BUNDLE", "DPD", "DPD_DELAY", "DPD_RETRY", "DPD_MAXFAIL",
  "PH1ID", "XAUTH_LOGIN", "WEAK_PHASE1_CHECK", "REKEY", "PREFIX", "PORT",
  "PORTANY", "UL_PROTO", "ANY", "IKE_FRAG", "ESP_FRAG", "MODE_CFG",
  "PFS_GROUP", "LIFETIME", "LIFETYPE_TIME", "LIFETYPE_BYTE", "STRENGTH",
  "REMOTEID", "SCRIPT", "PHASE1_UP", "PHASE1_DOWN", "PHASE1_DEAD",
  "NUMBER", "SWITCH", "BOOLEAN", "HEXSTRING", "QUOTEDSTRING", "ADDRSTRING",
  "ADDRRANGE", "UNITTYPE_BYTE", "UNITTYPE_KBYTES", "UNITTYPE_MBYTES",
  "UNITTYPE_TBYTES", "UNITTYPE_SEC", "UNITTYPE_MIN", "UNITTYPE_HOUR",
  "EOS", "BOC", "EOC", "COMMA", "REMOTE_ACCESS", "DYNAMIC_SITE",
  "SA_ASSOC", "USE_NATT_ADDR", "$accept", "statements", "statement",
  "privsep_statement", "privsep_stmts", "privsep_stmt", "$@1", "$@2",
  "$@3", "$@4", "$@5", "path_statement", "$@6", "special_statement", "$@7",
  "include_statement", "pfkey_statement", "gssenc_statement",
  "logging_statement", "log_level", "padding_statement", "padding_stmts",
  "padding_stmt", "$@8", "$@9", "$@10", "$@11", "$@12", "listen_statement",
  "listen_stmts", "listen_stmt", "$@13", "$@14", "$@15", "$@16", "$@17",
  "$@18", "ike_addrinfo_port", "ike_port", "radcfg_statement", "$@19",
  "radcfg_stmts", "radcfg_stmt", "$@20", "$@21", "$@22", "$@23", "$@24",
  "$@25", "ldapcfg_statement", "$@26", "ldapcfg_stmts", "ldapcfg_stmt",
  "$@27", "$@28", "$@29", "$@30", "$@31", "$@32", "$@33", "$@34", "$@35",
  "$@36", "$@37", "$@38", "modecfg_statement", "modecfg_stmts",
  "modecfg_stmt", "$@39", "$@40", "$@41", "$@42", "$@43", "$@44", "$@45",
  "$@46", "$@47", "$@48", "$@49", "$@50", "$@51", "$@52", "$@53", "$@54",
  "$@55", "$@56", "$@57", "$@58", "$@59", "$@60", "$@61", "$@62", "$@63",
  "addrdnslist", "addrdns", "addrwinslist", "addrwins", "splitnetlist",
  "splitnet", "authgrouplist", "authgroup", "splitdnslist", "splitdns",
  "timer_statement", "timer_stmts", "timer_stmt", "$@64", "$@65", "$@66",
  "$@67", "$@68", "$@69", "sainfo_statement", "$@70", "$@71",
  "sainfo_name", "sainfo_id", "sainfo_param", "sainfo_specs",
  "sainfo_spec", "$@72", "$@73", "$@74", "$@75", "$@76", "algorithms",
  "$@77", "algorithm", "prefix", "port", "ul_proto", "keylength",
  "remote_statement", "$@78", "$@79", "$@80", "$@81", "remote_specs_block",
  "remote_index", "remote_specs", "remote_spec", "$@82", "$@83", "$@84",
  "$@85", "$@86", "$@87", "$@88", "$@89", "$@90", "$@91", "$@92", "$@93",
  "$@94", "$@95", "$@96", "$@97", "$@98", "$@99", "$@100", "$@101",
  "$@102", "$@103", "$@104", "$@105", "$@106", "$@107", "$@108", "$@109",
  "$@110", "$@111", "$@112", "$@113", "$@114", "$@115", "$@116", "$@117",
  "$@118", "$@119", "$@120", "$@121", "$@122", "$@123", "$@124", "$@125",
  "$@126", "$@127", "$@128", "$@129", "$@130", "$@131", "$@132",
  "exchange_types", "cert_spec", "$@133", "$@134", "dh_group_num",
  "identifierstring", "isakmpproposal_specs", "isakmpproposal_spec",
  "$@135", "$@136", "$@137", "$@138", "$@139", "unittype_time",
  "unittype_byte", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   178,   179,   179,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   181,
     182,   182,   184,   183,   185,   183,   186,   183,   187,   183,
     188,   183,   190,   189,   192,   191,   193,   194,   195,   196,
     197,   198,   199,   199,   201,   200,   202,   200,   203,   200,
     204,   200,   205,   200,   206,   207,   207,   209,   208,   210,
     208,   211,   208,   212,   208,   213,   208,   214,   208,   215,
     216,   216,   218,   217,   219,   219,   221,   220,   222,   220,
     223,   220,   224,   220,   225,   220,   226,   220,   228,   227,
     229,   229,   231,   230,   232,   230,   233,   230,   234,   230,
     235,   230,   236,   230,   237,   230,   238,   230,   239,   230,
     240,   230,   241,   230,   242,   230,   243,   244,   244,   246,
     245,   247,   245,   245,   245,   248,   245,   249,   245,   250,
     245,   251,   245,   252,   245,   253,   245,   254,   245,   255,
     245,   256,   245,   257,   245,   258,   245,   259,   245,   260,
     245,   261,   245,   262,   245,   263,   245,   264,   245,   265,
     245,   266,   245,   267,   245,   268,   245,   269,   245,   270,
     245,   271,   271,   272,   273,   273,   274,   275,   275,   276,
     277,   277,   278,   279,   279,   280,   281,   282,   282,   284,
     283,   285,   283,   286,   283,   287,   283,   288,   283,   289,
     283,   291,   292,   290,   293,   293,   293,   293,   293,   293,
     294,   294,   294,   295,   295,   295,   296,   296,   298,   297,
     299,   297,   300,   297,   301,   297,   302,   297,   303,   304,
     303,   305,   306,   306,   307,   307,   307,   308,   308,   308,
     309,   309,   311,   310,   312,   310,   313,   310,   314,   310,
     315,   316,   316,   317,   317,   319,   318,   320,   318,   321,
     318,   322,   318,   318,   323,   318,   324,   318,   325,   318,
     326,   318,   327,   318,   328,   318,   329,   318,   330,   318,
     331,   318,   332,   318,   333,   318,   334,   318,   335,   318,
     336,   318,   337,   318,   338,   318,   339,   318,   340,   318,
     341,   318,   342,   318,   343,   318,   344,   318,   345,   318,
     346,   318,   347,   318,   348,   318,   349,   318,   350,   318,
     351,   318,   352,   318,   353,   318,   354,   318,   355,   318,
     356,   318,   357,   318,   358,   318,   359,   318,   360,   318,
     361,   318,   362,   318,   363,   318,   364,   318,   365,   318,
     366,   318,   367,   318,   368,   318,   369,   318,   370,   370,
     372,   371,   373,   371,   374,   374,   375,   375,   375,   376,
     376,   378,   377,   379,   377,   380,   377,   381,   377,   382,
     377,   383,   383,   383,   384,   384,   384,   384
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     4,
       0,     2,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     5,     0,     4,     3,     3,     3,     3,
       1,     4,     0,     2,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     4,     0,     2,     0,     4,     0,
       4,     0,     7,     0,     4,     0,     4,     0,     3,     2,
       0,     1,     0,     5,     0,     2,     0,     5,     0,     6,
       0,     5,     0,     6,     0,     4,     0,     4,     0,     5,
       0,     2,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     4,     0,     2,     0,
       4,     0,     4,     3,     3,     0,     5,     0,     5,     0,
       4,     0,     4,     0,     4,     0,     4,     0,     4,     0,
       4,     0,     4,     0,     4,     0,     4,     0,     4,     0,
       4,     0,     4,     0,     4,     0,     4,     0,     4,     0,
       4,     0,     4,     0,     4,     0,     4,     0,     4,     0,
       4,     1,     3,     1,     1,     3,     1,     1,     3,     2,
       1,     3,     1,     1,     3,     1,     4,     0,     2,     0,
       4,     0,     5,     0,     4,     0,     5,     0,     5,     0,
       5,     0,     0,     8,     1,     2,     2,     2,     2,     2,
       5,     6,     2,     0,     3,     2,     0,     2,     0,     4,
       0,     4,     0,     6,     0,     6,     0,     4,     1,     0,
       4,     2,     0,     1,     0,     1,     1,     1,     1,     1,
       0,     1,     0,     6,     0,     4,     0,     6,     0,     4,
       3,     2,     1,     0,     2,     0,     4,     0,     4,     0,
       4,     0,     4,     2,     0,     4,     0,     5,     0,     5,
       0,     4,     0,     5,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     5,     0,     6,     0,     4,     0,     5,
       0,     6,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     5,     0,     5,     0,     5,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     6,     0,     4,     0,     6,     0,     5,     0,     2,
       0,     5,     0,     4,     1,     1,     0,     1,     1,     0,
       2,     0,     6,     0,     6,     0,     4,     0,     4,     0,
       5,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     1,     0,     0,     0,     0,     0,     0,     0,
      88,    72,     0,     0,   201,     0,     0,     0,     3,     4,
       5,    18,     6,     7,     8,     9,    10,    11,    13,    12,
      14,    15,    16,    17,    20,     0,     0,     0,    40,     0,
      42,    55,     0,     0,   117,   187,     0,    70,   244,    70,
     252,   248,     0,    34,     0,    32,    36,    37,    39,     0,
       0,    90,    74,     0,     0,   204,     0,   213,     0,    71,
     251,     0,     0,    69,     0,     0,    38,     0,     0,     0,
       0,    19,    21,     0,     0,     0,     0,     0,     0,    41,
      43,     0,     0,    67,     0,    54,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   118,     0,     0,
       0,     0,     0,     0,   186,   188,   205,   206,   212,   232,
       0,     0,     0,   207,   208,   209,   242,   253,   245,   246,
     249,    35,    24,    22,    28,    26,    30,    33,    44,    46,
      48,    50,    52,    57,    59,     0,    65,    63,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      89,    91,     0,     0,     0,     0,    73,    75,   119,   121,
     173,     0,   171,   176,     0,   174,   131,   133,   135,   137,
     139,   182,   141,   180,   143,   145,   149,   151,   153,   147,
     165,   167,   163,   169,   155,   161,     0,     0,   185,   129,
     183,   157,   159,   189,     0,   193,     0,     0,     0,   233,
     232,   234,   215,   366,   216,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      68,     0,     0,     0,    94,    96,    92,    98,   102,   104,
     100,   106,   108,   110,   112,   114,     0,     0,    84,    86,
       0,     0,   123,     0,   124,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   125,   177,   127,     0,
       0,     0,     0,     0,   381,   382,   383,   191,     0,   195,
     197,   199,   234,   235,   236,     0,   368,   367,   214,   202,
     243,     0,   257,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   296,     0,     0,     0,
       0,     0,     0,   356,     0,     0,     0,     0,     0,   366,
       0,     0,     0,     0,     0,     0,     0,   250,     0,     0,
       0,     0,   254,   247,    25,    23,    29,    27,    31,    45,
      47,    49,    51,    53,    58,    60,    66,     0,    64,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    76,     0,    80,     0,     0,   120,   122,   172,
     175,   132,   134,   136,   138,   140,   142,   181,   144,   146,
     150,   152,   154,   148,   166,   168,   164,   170,   156,   162,
     179,     0,     0,     0,   130,   184,   158,   160,   190,     0,
     194,     0,     0,     0,     0,   238,   239,   237,   210,   226,
       0,     0,     0,     0,   217,   255,   358,   259,   261,     0,
       0,   263,   270,     0,     0,   264,     0,   274,   276,   278,
     280,   366,   366,   292,   294,     0,   298,   330,   334,   332,
     352,   326,   324,   328,     0,   336,   338,   340,   342,   348,
     286,   322,   346,   344,   310,   308,   312,   320,     0,     0,
       0,   300,   302,   304,   306,    61,    95,    97,    93,    99,
     103,   105,   101,   107,   109,   111,   113,   115,    78,     0,
      82,     0,    85,    87,   178,   126,   128,   192,   196,   198,
     200,   211,     0,   364,   365,   218,     0,     0,   220,   203,
       0,     0,     0,     0,     0,   362,     0,   266,   268,     0,
     272,     0,     0,     0,     0,   366,   282,   366,   288,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     369,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   314,   316,   318,     0,
       0,     0,     0,     0,     0,    77,     0,    81,   240,     0,
     228,     0,     0,     0,     0,   256,   359,   258,   260,   262,
     360,     0,   271,     0,     0,   265,     0,   275,   277,   279,
     281,   284,     0,   290,     0,   293,   295,   297,   299,   331,
     335,   333,   353,   327,   325,   329,     0,   337,   339,   341,
     343,   349,   287,   323,   347,   345,   311,   309,   313,   321,
     350,   384,   385,   386,   387,   354,     0,     0,     0,   301,
     303,   305,   307,    62,    79,    83,   241,   231,   227,     0,
     219,   222,   224,   221,     0,   363,   267,   269,   273,     0,
     283,     0,   289,     0,     0,     0,     0,   357,   370,     0,
       0,   315,   317,   319,     0,     0,     0,   361,   285,   291,
     240,   375,   377,     0,     0,   351,   355,   230,   223,   225,
     379,     0,     0,     0,     0,     0,   376,   378,   371,   373,
     380,     0,     0,   372,   374
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    18,    19,    54,    82,   229,   228,   231,   230,
     232,    20,    83,    21,    77,    22,    23,    24,    25,    39,
      26,    59,    90,   233,   234,   235,   236,   237,    27,    60,
      96,   238,   239,   573,   243,   241,   155,    50,    70,    28,
      43,    98,   177,   499,   574,   501,   576,   385,   386,    29,
      42,    97,   171,   371,   369,   370,   372,   375,   373,   374,
     376,   377,   378,   379,   380,    30,    63,   117,   260,   261,
     412,   413,   289,   266,   267,   268,   269,   270,   271,   273,
     274,   278,   275,   276,   277,   283,   291,   292,   284,   281,
     279,   280,   282,   181,   182,   184,   185,   286,   287,   192,
     193,   209,   210,    31,    64,   125,   293,   419,   298,   421,
     422,   423,    32,    46,   433,    67,    68,   132,   309,   434,
     581,   584,   675,   676,   512,   579,   649,   580,   221,   305,
     428,   647,    33,   225,    72,   227,    75,   138,    51,   226,
     352,   520,   436,   522,   523,   529,   593,   594,   526,   596,
     531,   532,   533,   534,   602,   659,   556,   604,   661,   539,
     540,   455,   542,   569,   570,   571,   572,   561,   560,   562,
     636,   637,   638,   563,   557,   548,   547,   549,   543,   545,
     544,   551,   552,   553,   554,   559,   558,   555,   669,   546,
     670,   464,   521,   441,   654,   591,   515,   308,   616,   668,
     701,   702,   691,   692,   695,   297,   635
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -547
static const yytype_int16 yypact[] =
{
    -547,    42,  -547,  -114,    51,   -90,   -71,    91,    22,    25,
    -547,  -547,    28,    32,  -547,   -45,     6,   -15,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,    -2,    30,    35,  -547,    37,
    -547,  -547,    39,    41,  -547,  -547,   -29,    67,    79,    67,
    -547,   131,    54,  -547,     3,  -547,  -547,  -547,  -547,    -4,
      -5,  -547,  -547,    29,    -9,   -28,   -66,    36,    45,  -547,
    -547,    68,    58,  -547,   -36,    58,  -547,    60,   -33,   -23,
      73,  -547,  -547,    69,    80,    84,    88,    90,    92,  -547,
    -547,    85,    85,  -547,    -8,  -547,  -547,    -6,    -7,    87,
      99,   100,   101,   103,   102,   104,    89,    78,   106,   110,
      94,    95,   108,   111,   112,   116,  -547,  -547,   119,   120,
     121,   123,   124,   125,  -547,  -547,  -547,  -547,  -547,   -78,
     118,   144,   113,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,   117,  -547,   126,   128,   127,
     129,   130,   132,   133,   134,   135,   140,   141,   142,   143,
    -547,  -547,   145,   146,   148,   151,  -547,  -547,  -547,  -547,
    -547,   138,   109,  -547,   139,   137,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,   147,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,   150,   150,  -547,  -547,
     149,  -547,  -547,  -547,   -18,  -547,   -18,   -18,   -18,  -547,
     174,    48,  -547,    21,  -547,    58,   122,    58,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
    -547,   165,   176,   167,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,   -20,   -13,  -547,  -547,
     168,   169,  -547,   100,  -547,   101,   170,   171,   172,   173,
     175,   177,   104,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   178,   191,  -547,   191,   192,
     111,   193,   195,   197,  -547,  -547,  -547,  -547,   198,  -547,
    -547,  -547,    48,  -547,  -547,    -3,  -547,  -547,  -547,   -37,
    -547,    85,  -547,   199,   221,    82,     0,   207,   204,   212,
     213,   214,   216,   217,   215,   218,  -547,   219,   220,   -57,
     202,   -75,   222,  -547,   223,   225,   226,   227,   228,    21,
     229,   -46,   -44,   231,   232,    43,   230,  -547,   234,   235,
     236,   237,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,   239,  -547,   203,
     205,   208,   233,   238,   240,   241,   242,   243,   244,   245,
     246,   247,  -547,   249,  -547,   248,   250,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,   150,   251,   252,  -547,  -547,  -547,  -547,  -547,   253,
    -547,   254,   255,   256,    -3,  -547,  -547,  -547,  -547,  -547,
     -30,    46,   261,   224,  -547,  -547,  -547,  -547,  -547,   259,
     267,  -547,  -547,   268,   269,  -547,   270,  -547,  -547,  -547,
    -547,   -59,   -56,  -547,  -547,   -30,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,   260,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,   276,   277,
      31,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,   264,
    -547,   265,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,   194,  -547,  -547,  -547,   280,   281,  -547,  -547,
     271,   -49,   272,   273,   278,  -547,   274,  -547,  -547,   275,
    -547,   279,   282,   283,   284,    21,  -547,    21,  -547,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
    -547,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   308,   309,   -18,    13,  -547,  -547,  -547,   310,
     311,   312,   313,   314,   315,  -547,   316,  -547,   331,   318,
     266,   319,   -18,    13,   320,  -547,  -547,  -547,  -547,  -547,
    -547,   321,  -547,   322,   323,  -547,   324,  -547,  -547,  -547,
    -547,  -547,   325,  -547,   326,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,   -27,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,   327,   328,   329,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,   330,
    -547,  -547,  -547,  -547,   332,  -547,  -547,  -547,  -547,   334,
    -547,   335,  -547,   307,   -30,   340,    49,  -547,  -547,   336,
     337,  -547,  -547,  -547,   194,   338,   339,  -547,  -547,  -547,
     331,  -547,  -547,   345,   354,  -547,  -547,  -547,  -547,  -547,
    -547,   341,   342,   -18,    13,   343,  -547,  -547,  -547,  -547,
    -547,   344,   346,  -547,  -547
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,   -88,   348,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,    52,  -547,    56,  -547,   317,   -67,    74,
    -547,    98,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,   115,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -276,  -547,  -547,   333,    97,
     -24,  -279,  -547,  -547,  -547,  -547,  -547,   -55,   366,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -547,  -450,  -333,  -547,  -547,
    -547,  -547,  -547,  -547,  -547,  -216,  -546
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -230
static const yytype_int16 yytable[] =
{
     299,   300,   301,   153,   154,   541,   470,    78,    79,    80,
      84,    85,    86,    87,    88,    91,    92,   156,    93,    94,
     140,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,   168,   169,   172,   173,   174,   175,   652,    47,   586,
     429,   130,     2,   535,   461,     3,   537,    47,   513,     4,
     663,     5,     6,     7,    65,     8,   126,    34,   458,    35,
     219,     9,   118,   119,   120,   121,   122,   123,    10,   472,
      36,   474,    66,    66,    99,   100,   101,   102,   103,   104,
     105,    11,   462,   664,   220,    37,    12,   106,   107,   108,
     109,   110,   111,   112,   128,   129,   113,   114,   115,   665,
     459,   306,   307,    38,   306,   307,   442,   443,   444,   430,
     431,   473,    13,   475,   432,    48,    49,   131,   536,   538,
     666,   587,    14,   142,    15,    49,   514,   143,   133,   134,
     196,   197,   198,   144,    52,   199,   381,   145,   425,   426,
     382,   194,    53,   383,   195,   667,    66,   384,   699,   294,
     295,   296,   157,   427,   187,   188,   189,   190,    55,   200,
     445,   201,   202,   124,    71,   176,   170,    95,    89,    16,
     310,    17,   353,   206,   207,    81,   631,   632,   633,   634,
     127,   306,   307,   135,   566,   567,   568,   303,   304,   439,
     440,   478,   479,    40,   516,   517,    41,   683,   684,    44,
      56,   116,   601,    45,   603,    57,    69,    58,   311,   312,
      61,   313,    62,   314,   681,   315,    74,   316,   317,   318,
     319,   320,   321,   435,    76,   322,   323,   324,   136,   137,
     141,   325,   326,   146,   327,   328,   329,   148,   330,   147,
     331,   149,   332,   333,   150,   223,    49,   151,   178,   152,
     204,   205,   334,   335,   336,   337,   338,   339,   340,   341,
     179,   180,   183,   186,   191,   342,   343,   344,   211,   345,
     203,   208,   578,   212,   346,   213,   214,   215,   222,   216,
     217,   218,   263,   245,   224,   246,   242,   240,   244,   437,
     247,   250,   248,   249,   347,   251,   348,   349,   350,   351,
     252,   253,   254,   255,   258,   256,   257,   259,   262,   264,
     265,   285,   219,   438,   446,   389,   410,   451,   452,   460,
     272,   390,   290,   354,   355,   356,   357,   358,   359,   360,
     361,   362,   363,   364,   365,   366,   367,   368,   387,   388,
     391,   392,   393,   394,   504,   395,   397,   396,   630,   398,
     399,   400,   401,   402,   403,   404,   405,   406,   407,   408,
     409,   447,   414,   416,   411,   417,   651,   418,   420,   448,
     449,   450,   453,   486,   454,   487,   456,   457,   488,   463,
     465,   466,   467,   468,   469,   680,   471,   476,   415,   477,
     480,   481,   482,   483,   484,   485,   519,    73,   687,   424,
     511,   690,     0,   489,     0,     0,     0,   498,   490,   500,
     491,   492,   493,   494,   495,   496,   497,   518,   502,   524,
     503,   505,   506,   507,   508,   509,   510,   525,   527,   528,
     530,   550,   564,   565,   575,   577,   582,   583,   590,  -229,
     139,   585,   588,   589,   592,   595,     0,     0,     0,   597,
       0,     0,   598,   599,   600,   605,   606,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   617,   618,   619,   620,
     621,   622,   623,   624,   625,   626,   627,   698,   628,   629,
     639,   640,   641,   642,   643,   644,   645,   646,   648,   650,
     653,   655,   656,   657,   658,   660,   662,   671,   672,   673,
     682,   693,   677,   674,   678,   679,   685,   686,   688,   689,
     694,   696,   697,   700,   703,     0,   704,     0,     0,     0,
       0,     0,     0,     0,   288,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   302
};

static const yytype_int16 yycheck[] =
{
     216,   217,   218,    91,    92,   455,   339,     4,     5,     6,
      14,    15,    16,    17,    18,    20,    21,    25,    23,    24,
      75,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    40,    41,    42,    43,   583,    83,    88,
      77,     5,     0,   102,   119,     3,   102,    83,    78,     7,
      77,     9,    10,    11,    83,    13,    84,   171,   115,     8,
     138,    19,    71,    72,    73,    74,    75,    76,    26,   115,
     160,   115,   101,   101,    45,    46,    47,    48,    49,    50,
      51,    39,   157,   110,   162,   156,    44,    58,    59,    60,
      61,    62,    63,    64,   160,   161,    67,    68,    69,   126,
     157,   160,   161,    12,   160,   161,   106,   107,   108,   146,
     147,   157,    70,   157,   151,   160,   161,    81,   451,   452,
     147,   170,    80,   156,    82,   161,   156,   160,    83,    84,
      52,    53,    54,   156,   128,    57,   156,   160,   141,   142,
     160,    52,   157,   156,    55,   172,   101,   160,   694,   167,
     168,   169,   160,   156,    52,    53,    54,    55,   160,    53,
     160,    55,    56,   172,    85,   172,   172,   172,   172,   127,
     225,   129,   227,    65,    66,   172,   163,   164,   165,   166,
      65,   160,   161,    68,   153,   154,   155,   139,   140,   107,
     108,   148,   149,   171,   148,   149,   171,   148,   149,   171,
     170,   172,   535,   171,   537,   170,   139,   170,    86,    87,
     171,    89,   171,    91,   664,    93,    85,    95,    96,    97,
      98,    99,   100,   311,   170,   103,   104,   105,   160,   171,
     170,   109,   110,   160,   112,   113,   114,   157,   116,   170,
     118,   157,   120,   121,   156,   101,   161,   157,   161,   157,
     156,   156,   130,   131,   132,   133,   134,   135,   136,   137,
     161,   161,   161,   160,   160,   143,   144,   145,   156,   147,
     160,   160,    78,   157,   152,   156,   156,   156,   160,   156,
     156,   156,   173,   156,   171,   156,   160,   170,   160,    90,
     160,   157,   160,   160,   172,   160,   174,   175,   176,   177,
     160,   160,   160,   160,   156,   160,   160,   156,   170,   170,
     173,   161,   138,    92,   107,   263,   138,   101,   101,   117,
     173,   265,   173,   170,   170,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   160,   170,   170,   170,
     170,   170,   170,   170,   411,   170,   272,   170,   564,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   170,   170,
     170,   157,   170,   170,   173,   170,   582,   170,   170,   157,
     157,   157,   157,   170,   156,   170,   157,   157,   170,   157,
     157,   156,   156,   156,   156,    78,   157,   156,   290,   157,
     160,   157,   157,   157,   157,   156,   172,    49,   674,   302,
     424,   680,    -1,   170,    -1,    -1,    -1,   160,   170,   160,
     170,   170,   170,   170,   170,   170,   170,   156,   170,   160,
     170,   170,   170,   170,   170,   170,   170,   160,   160,   160,
     160,   171,   156,   156,   170,   170,   156,   156,   160,   173,
      74,   170,   170,   170,   170,   170,    -1,    -1,    -1,   170,
      -1,    -1,   170,   170,   170,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   693,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   156,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   170,   170,
     160,   156,   170,   173,   170,   170,   170,   170,   170,   170,
     156,   170,   170,   170,   170,    -1,   170,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   207,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   220
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   179,     0,     3,     7,     9,    10,    11,    13,    19,
      26,    39,    44,    70,    80,    82,   127,   129,   180,   181,
     189,   191,   193,   194,   195,   196,   198,   206,   217,   227,
     243,   281,   290,   310,   171,     8,   160,   156,    12,   197,
     171,   171,   228,   218,   171,   171,   291,    83,   160,   161,
     215,   316,   128,   157,   182,   160,   170,   170,   170,   199,
     207,   171,   171,   244,   282,    83,   101,   293,   294,   139,
     216,    85,   312,   216,    85,   314,   170,   192,     4,     5,
       6,   172,   183,   190,    14,    15,    16,    17,    18,   172,
     200,    20,    21,    23,    24,   172,   208,   229,   219,    45,
      46,    47,    48,    49,    50,    51,    58,    59,    60,    61,
      62,    63,    64,    67,    68,    69,   172,   245,    71,    72,
      73,    74,    75,    76,   172,   283,    84,   294,   160,   161,
       5,    81,   295,    83,    84,   294,   160,   171,   315,   316,
     315,   170,   156,   160,   156,   160,   160,   170,   157,   157,
     156,   157,   157,   215,   215,   214,    25,   160,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
     172,   230,    40,    41,    42,    43,   172,   220,   161,   161,
     161,   271,   272,   161,   273,   274,   160,    52,    53,    54,
      55,   160,   277,   278,    52,    55,    52,    53,    54,    57,
      53,    55,    56,   160,   156,   156,    65,    66,   160,   279,
     280,   156,   157,   156,   156,   156,   156,   156,   156,   138,
     162,   306,   160,   101,   171,   311,   317,   313,   185,   184,
     187,   186,   188,   201,   202,   203,   204,   205,   209,   210,
     170,   213,   160,   212,   160,   156,   156,   160,   160,   160,
     157,   160,   160,   160,   160,   160,   160,   160,   156,   156,
     246,   247,   170,   173,   170,   173,   251,   252,   253,   254,
     255,   256,   173,   257,   258,   260,   261,   262,   259,   268,
     269,   267,   270,   263,   266,   161,   275,   276,   275,   250,
     173,   264,   265,   284,   167,   168,   169,   383,   286,   383,
     383,   383,   306,   139,   140,   307,   160,   161,   375,   296,
     315,    86,    87,    89,    91,    93,    95,    96,    97,    98,
      99,   100,   103,   104,   105,   109,   110,   112,   113,   114,
     116,   118,   120,   121,   130,   131,   132,   133,   134,   135,
     136,   137,   143,   144,   145,   147,   152,   172,   174,   175,
     176,   177,   318,   315,   170,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   160,   170,   232,
     233,   231,   234,   236,   237,   235,   238,   239,   240,   241,
     242,   156,   160,   156,   160,   225,   226,   170,   170,   271,
     273,   170,   170,   170,   170,   170,   170,   277,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   170,   170,
     138,   173,   248,   249,   170,   279,   170,   170,   170,   285,
     170,   287,   288,   289,   307,   141,   142,   156,   308,    77,
     146,   147,   151,   292,   297,   215,   320,    90,    92,   107,
     108,   371,   106,   107,   108,   160,   107,   157,   157,   157,
     157,   101,   101,   157,   156,   339,   157,   157,   115,   157,
     117,   119,   157,   157,   369,   157,   156,   156,   156,   156,
     375,   157,   115,   157,   115,   157,   156,   157,   148,   149,
     160,   157,   157,   157,   157,   156,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   160,   221,
     160,   223,   170,   170,   276,   170,   170,   170,   170,   170,
     170,   308,   302,    78,   156,   374,   148,   149,   156,   172,
     319,   370,   321,   322,   160,   160,   326,   160,   160,   323,
     160,   328,   329,   330,   331,   102,   375,   102,   375,   337,
     338,   374,   340,   356,   358,   357,   367,   354,   353,   355,
     171,   359,   360,   361,   362,   365,   334,   352,   364,   363,
     346,   345,   347,   351,   156,   156,   153,   154,   155,   341,
     342,   343,   344,   211,   222,   170,   224,   170,    78,   303,
     305,   298,   156,   156,   299,   170,    88,   170,   170,   170,
     160,   373,   170,   324,   325,   170,   327,   170,   170,   170,
     170,   375,   332,   375,   335,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   376,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   170,   170,
     383,   163,   164,   165,   166,   384,   348,   349,   350,   170,
     170,   170,   170,   170,   170,   170,   156,   309,   170,   304,
     170,   383,   384,   170,   372,   170,   170,   170,   170,   333,
     170,   336,   170,    77,   110,   126,   147,   172,   377,   366,
     368,   170,   170,   170,   173,   300,   301,   170,   170,   170,
      78,   374,   160,   148,   149,   170,   170,   303,   170,   170,
     309,   380,   381,   156,   156,   382,   170,   170,   383,   384,
     170,   378,   379,   170,   170
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 22:

/* Line 1455 of yacc.c  */
#line 302 "cfparse.y"
    {
			struct passwd *pw;

			if ((pw = getpwnam((yyvsp[(2) - (2)].val)->v)) == NULL) {
				yyerror("unknown user \"%s\"", (yyvsp[(2) - (2)].val)->v);
				return -1;
			}
			lcconf->uid = pw->pw_uid;
		}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 312 "cfparse.y"
    { lcconf->uid = (yyvsp[(2) - (2)].num); }
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 314 "cfparse.y"
    {
			struct group *gr;

			if ((gr = getgrnam((yyvsp[(2) - (2)].val)->v)) == NULL) {
				yyerror("unknown group \"%s\"", (yyvsp[(2) - (2)].val)->v);
				return -1;
			}
			lcconf->gid = gr->gr_gid;
		}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 324 "cfparse.y"
    { lcconf->gid = (yyvsp[(2) - (2)].num); }
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 325 "cfparse.y"
    { lcconf->chroot = (yyvsp[(2) - (2)].val)->v; }
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 331 "cfparse.y"
    {
			if ((yyvsp[(2) - (3)].num) >= LC_PATHTYPE_MAX) {
				yyerror("invalid path type %d", (yyvsp[(2) - (3)].num));
				return -1;
			}

			/* free old pathinfo */
			if (lcconf->pathinfo[(yyvsp[(2) - (3)].num)])
				racoon_free(lcconf->pathinfo[(yyvsp[(2) - (3)].num)]);

			/* set new pathinfo */
			lcconf->pathinfo[(yyvsp[(2) - (3)].num)] = racoon_strdup((yyvsp[(3) - (3)].val)->v);
			STRDUP_FATAL(lcconf->pathinfo[(yyvsp[(2) - (3)].num)]);
			vfree((yyvsp[(3) - (3)].val));
		}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 351 "cfparse.y"
    { lcconf->complex_bundle = (yyvsp[(2) - (2)].num); }
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 357 "cfparse.y"
    {
			char path[MAXPATHLEN];

			getpathname(path, sizeof(path),
				LC_PATHTYPE_INCLUDE, (yyvsp[(2) - (3)].val)->v);
			vfree((yyvsp[(2) - (3)].val));
			if (yycf_switch_buffer(path) != 0)
				return -1;
		}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 371 "cfparse.y"
    {
			lcconf->pfkey_buffer_size = (yyvsp[(2) - (3)].num);
        }
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 378 "cfparse.y"
    {
			if ((yyvsp[(2) - (3)].num) >= LC_GSSENC_MAX) {
				yyerror("invalid GSS ID encoding %d", (yyvsp[(2) - (3)].num));
				return -1;
			}
			lcconf->gss_id_enc = (yyvsp[(2) - (3)].num);
		}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 393 "cfparse.y"
    {
			/*
			 * set the loglevel to the value specified
			 * in the configuration file plus the number
			 * of -d options specified on the command line
			 */
			loglevel += (yyvsp[(1) - (1)].num) - oldloglevel;
			oldloglevel = (yyvsp[(1) - (1)].num);
		}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 413 "cfparse.y"
    { lcconf->pad_random = (yyvsp[(2) - (2)].num); }
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 414 "cfparse.y"
    { lcconf->pad_randomlen = (yyvsp[(2) - (2)].num); }
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 415 "cfparse.y"
    { lcconf->pad_maxsize = (yyvsp[(2) - (2)].num); }
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 416 "cfparse.y"
    { lcconf->pad_strict = (yyvsp[(2) - (2)].num); }
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 417 "cfparse.y"
    { lcconf->pad_excltail = (yyvsp[(2) - (2)].num); }
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 430 "cfparse.y"
    {
			myaddr_listen((yyvsp[(2) - (2)].saddr), FALSE);
			racoon_free((yyvsp[(2) - (2)].saddr));
		}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 436 "cfparse.y"
    {
#ifdef ENABLE_NATT
			myaddr_listen((yyvsp[(2) - (2)].saddr), TRUE);
			racoon_free((yyvsp[(2) - (2)].saddr));
#else
			racoon_free((yyvsp[(2) - (2)].saddr));
			yyerror("NAT-T support not compiled in.");
#endif
		}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 447 "cfparse.y"
    {
#ifdef ENABLE_ADMINPORT
			adminsock_conf((yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].num));
#else
			yywarn("admin port support not compiled in");
#endif
		}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 456 "cfparse.y"
    {
#ifdef ENABLE_ADMINPORT
			adminsock_conf((yyvsp[(2) - (2)].val), NULL, NULL, -1);
#else
			yywarn("admin port support not compiled in");
#endif
		}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 465 "cfparse.y"
    {
#ifdef ENABLE_ADMINPORT
			adminsock_path = NULL;
#else
			yywarn("admin port support not compiled in");
#endif
		}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 473 "cfparse.y"
    { lcconf->strict_address = TRUE; }
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 477 "cfparse.y"
    {
			char portbuf[10];

			snprintf(portbuf, sizeof(portbuf), "%ld", (yyvsp[(2) - (2)].num));
			(yyval.saddr) = str2saddr((yyvsp[(1) - (2)].val)->v, portbuf);
			vfree((yyvsp[(1) - (2)].val));
			if (!(yyval.saddr))
				return -1;
		}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 488 "cfparse.y"
    { (yyval.num) = PORT_ISAKMP; }
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 489 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 494 "cfparse.y"
    {
#ifndef ENABLE_HYBRID
			yyerror("racoon not configured with --enable-hybrid");
			return -1;
#endif
#ifndef HAVE_LIBRADIUS
			yyerror("racoon not configured with --with-libradius");
			return -1;
#endif
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			xauth_rad_config.timeout = 3;
			xauth_rad_config.retries = 3;
#endif
#endif
		}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 517 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			int i = xauth_rad_config.auth_server_count;
			if (i == RADIUS_MAX_SERVERS) {
				yyerror("maximum radius auth servers exceeded");
				return -1;
			}

			xauth_rad_config.auth_server_list[i].host = vdup((yyvsp[(2) - (3)].val));
			xauth_rad_config.auth_server_list[i].secret = vdup((yyvsp[(3) - (3)].val));
			xauth_rad_config.auth_server_list[i].port = 0; // default port
			xauth_rad_config.auth_server_count++;
#endif
#endif
		}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 535 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			int i = xauth_rad_config.auth_server_count;
			if (i == RADIUS_MAX_SERVERS) {
				yyerror("maximum radius auth servers exceeded");
				return -1;
			}

			xauth_rad_config.auth_server_list[i].host = vdup((yyvsp[(2) - (4)].val));
			xauth_rad_config.auth_server_list[i].secret = vdup((yyvsp[(4) - (4)].val));
			xauth_rad_config.auth_server_list[i].port = (yyvsp[(3) - (4)].num);
			xauth_rad_config.auth_server_count++;
#endif
#endif
		}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 553 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			int i = xauth_rad_config.acct_server_count;
			if (i == RADIUS_MAX_SERVERS) {
				yyerror("maximum radius account servers exceeded");
				return -1;
			}

			xauth_rad_config.acct_server_list[i].host = vdup((yyvsp[(2) - (3)].val));
			xauth_rad_config.acct_server_list[i].secret = vdup((yyvsp[(3) - (3)].val));
			xauth_rad_config.acct_server_list[i].port = 0; // default port
			xauth_rad_config.acct_server_count++;
#endif
#endif
		}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 571 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			int i = xauth_rad_config.acct_server_count;
			if (i == RADIUS_MAX_SERVERS) {
				yyerror("maximum radius account servers exceeded");
				return -1;
			}

			xauth_rad_config.acct_server_list[i].host = vdup((yyvsp[(2) - (4)].val));
			xauth_rad_config.acct_server_list[i].secret = vdup((yyvsp[(4) - (4)].val));
			xauth_rad_config.acct_server_list[i].port = (yyvsp[(3) - (4)].num);
			xauth_rad_config.acct_server_count++;
#endif
#endif
		}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 589 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			xauth_rad_config.timeout = (yyvsp[(2) - (2)].num);
#endif
#endif
		}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 598 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			xauth_rad_config.retries = (yyvsp[(2) - (2)].num);
#endif
#endif
		}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 610 "cfparse.y"
    {
#ifndef ENABLE_HYBRID
			yyerror("racoon not configured with --enable-hybrid");
			return -1;
#endif
#ifndef HAVE_LIBLDAP
			yyerror("racoon not configured with --with-libldap");
			return -1;
#endif
		}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 627 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (((yyvsp[(2) - (2)].num)<2)||((yyvsp[(2) - (2)].num)>3))
				yyerror("invalid ldap protocol version (2|3)");
			xauth_ldap_config.pver = (yyvsp[(2) - (2)].num);
#endif
#endif
		}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 638 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.host != NULL)
				vfree(xauth_ldap_config.host);
			xauth_ldap_config.host = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 649 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			xauth_ldap_config.port = (yyvsp[(2) - (2)].num);
#endif
#endif
		}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 658 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.base != NULL)
				vfree(xauth_ldap_config.base);
			xauth_ldap_config.base = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 669 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			xauth_ldap_config.subtree = (yyvsp[(2) - (2)].num);
#endif
#endif
		}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 678 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.bind_dn != NULL)
				vfree(xauth_ldap_config.bind_dn);
			xauth_ldap_config.bind_dn = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 689 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.bind_pw != NULL)
				vfree(xauth_ldap_config.bind_pw);
			xauth_ldap_config.bind_pw = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 700 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.attr_user != NULL)
				vfree(xauth_ldap_config.attr_user);
			xauth_ldap_config.attr_user = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 711 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.attr_addr != NULL)
				vfree(xauth_ldap_config.attr_addr);
			xauth_ldap_config.attr_addr = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 722 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.attr_mask != NULL)
				vfree(xauth_ldap_config.attr_mask);
			xauth_ldap_config.attr_mask = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 733 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.attr_group != NULL)
				vfree(xauth_ldap_config.attr_group);
			xauth_ldap_config.attr_group = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 744 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			if (xauth_ldap_config.attr_member != NULL)
				vfree(xauth_ldap_config.attr_member);
			xauth_ldap_config.attr_member = vdup((yyvsp[(2) - (2)].val));
#endif
#endif
		}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 766 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			if (inet_pton(AF_INET, (yyvsp[(2) - (2)].val)->v,
			     &isakmp_cfg_config.network4) != 1)
				yyerror("bad IPv4 network address.");
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 777 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			if (inet_pton(AF_INET, (yyvsp[(2) - (2)].val)->v,
			    &isakmp_cfg_config.netmask4) != 1)
				yyerror("bad IPv4 netmask address.");
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 792 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.splitnet_type = UNITY_LOCAL_LAN;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 801 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.splitnet_type = UNITY_SPLIT_INCLUDE;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 810 "cfparse.y"
    {
#ifndef ENABLE_HYBRID
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 817 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			strncpy(&isakmp_cfg_config.default_domain[0], 
			    (yyvsp[(2) - (2)].val)->v, MAXPATHLEN);
			isakmp_cfg_config.default_domain[MAXPATHLEN] = '\0';
			vfree((yyvsp[(2) - (2)].val));
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 829 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.authsource = ISAKMP_CFG_AUTH_SYSTEM;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 838 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			isakmp_cfg_config.authsource = ISAKMP_CFG_AUTH_RADIUS;
#else /* HAVE_LIBRADIUS */
			yyerror("racoon not configured with --with-libradius");
#endif /* HAVE_LIBRADIUS */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 851 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBPAM
			isakmp_cfg_config.authsource = ISAKMP_CFG_AUTH_PAM;
#else /* HAVE_LIBPAM */
			yyerror("racoon not configured with --with-libpam");
#endif /* HAVE_LIBPAM */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 864 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			isakmp_cfg_config.authsource = ISAKMP_CFG_AUTH_LDAP;
#else /* HAVE_LIBLDAP */
			yyerror("racoon not configured with --with-libldap");
#endif /* HAVE_LIBLDAP */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 877 "cfparse.y"
    {
#ifndef ENABLE_HYBRID
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 884 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.groupsource = ISAKMP_CFG_GROUP_SYSTEM;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 893 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			isakmp_cfg_config.groupsource = ISAKMP_CFG_GROUP_LDAP;
#else /* HAVE_LIBLDAP */
			yyerror("racoon not configured with --with-libldap");
#endif /* HAVE_LIBLDAP */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 906 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.accounting = ISAKMP_CFG_ACCT_NONE;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 915 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.accounting = ISAKMP_CFG_ACCT_SYSTEM;
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 151:

/* Line 1455 of yacc.c  */
#line 924 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			isakmp_cfg_config.accounting = ISAKMP_CFG_ACCT_RADIUS;
#else /* HAVE_LIBRADIUS */
			yyerror("racoon not configured with --with-libradius");
#endif /* HAVE_LIBRADIUS */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 153:

/* Line 1455 of yacc.c  */
#line 937 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBPAM
			isakmp_cfg_config.accounting = ISAKMP_CFG_ACCT_PAM;
#else /* HAVE_LIBPAM */
			yyerror("racoon not configured with --with-libpam");
#endif /* HAVE_LIBPAM */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 155:

/* Line 1455 of yacc.c  */
#line 950 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			if (isakmp_cfg_resize_pool((yyvsp[(2) - (2)].num)) != 0)
				yyerror("cannot allocate memory for pool");
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 157:

/* Line 1455 of yacc.c  */
#line 960 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.pfs_group = (yyvsp[(2) - (2)].num);
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 159:

/* Line 1455 of yacc.c  */
#line 969 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.save_passwd = (yyvsp[(2) - (2)].num);
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 161:

/* Line 1455 of yacc.c  */
#line 978 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.auth_throttle = (yyvsp[(2) - (2)].num);
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 163:

/* Line 1455 of yacc.c  */
#line 987 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			isakmp_cfg_config.confsource = ISAKMP_CFG_CONF_LOCAL;
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 165:

/* Line 1455 of yacc.c  */
#line 996 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBRADIUS
			isakmp_cfg_config.confsource = ISAKMP_CFG_CONF_RADIUS;
#else /* HAVE_LIBRADIUS */
			yyerror("racoon not configured with --with-libradius");
#endif /* HAVE_LIBRADIUS */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 167:

/* Line 1455 of yacc.c  */
#line 1009 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
#ifdef HAVE_LIBLDAP
			isakmp_cfg_config.confsource = ISAKMP_CFG_CONF_LDAP;
#else /* HAVE_LIBLDAP */
			yyerror("racoon not configured with --with-libldap");
#endif /* HAVE_LIBLDAP */
#else /* ENABLE_HYBRID */
			yyerror("racoon not configured with --enable-hybrid");
#endif /* ENABLE_HYBRID */
		}
    break;

  case 169:

/* Line 1455 of yacc.c  */
#line 1022 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			strncpy(&isakmp_cfg_config.motd[0], (yyvsp[(2) - (2)].val)->v, MAXPATHLEN);
			isakmp_cfg_config.motd[MAXPATHLEN] = '\0';
			vfree((yyvsp[(2) - (2)].val));
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 173:

/* Line 1455 of yacc.c  */
#line 1040 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			struct isakmp_cfg_config *icc = &isakmp_cfg_config;

			if (icc->dns4_index > MAXNS)
				yyerror("No more than %d DNS", MAXNS);
			if (inet_pton(AF_INET, (yyvsp[(1) - (1)].val)->v,
			    &icc->dns4[icc->dns4_index++]) != 1)
				yyerror("bad IPv4 DNS address.");
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 176:

/* Line 1455 of yacc.c  */
#line 1061 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			struct isakmp_cfg_config *icc = &isakmp_cfg_config;

			if (icc->nbns4_index > MAXWINS)
				yyerror("No more than %d WINS", MAXWINS);
			if (inet_pton(AF_INET, (yyvsp[(1) - (1)].val)->v,
			    &icc->nbns4[icc->nbns4_index++]) != 1)
				yyerror("bad IPv4 WINS address.");
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 179:

/* Line 1455 of yacc.c  */
#line 1082 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			struct isakmp_cfg_config *icc = &isakmp_cfg_config;
			struct unity_network network;
			memset(&network,0,sizeof(network));

			if (inet_pton(AF_INET, (yyvsp[(1) - (2)].val)->v, &network.addr4) != 1)
				yyerror("bad IPv4 SPLIT address.");

			/* Turn $2 (the prefix) into a subnet mask */
			network.mask4.s_addr = ((yyvsp[(2) - (2)].num)) ? htonl(~((1 << (32 - (yyvsp[(2) - (2)].num))) - 1)) : 0;

			/* add the network to our list */ 
			if (splitnet_list_add(&icc->splitnet_list, &network,&icc->splitnet_count))
				yyerror("Unable to allocate split network");
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 182:

/* Line 1455 of yacc.c  */
#line 1109 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			char * groupname = NULL;
			char ** grouplist = NULL;
			struct isakmp_cfg_config *icc = &isakmp_cfg_config;

			grouplist = racoon_realloc(icc->grouplist,
					sizeof(char**)*(icc->groupcount+1));
			if (grouplist == NULL) {
				yyerror("unable to allocate auth group list");
				return -1;
			}

			groupname = racoon_malloc((yyvsp[(1) - (1)].val)->l+1);
			if (groupname == NULL) {
				yyerror("unable to allocate auth group name");
				return -1;
			}

			memcpy(groupname,(yyvsp[(1) - (1)].val)->v,(yyvsp[(1) - (1)].val)->l);
			groupname[(yyvsp[(1) - (1)].val)->l]=0;
			grouplist[icc->groupcount]=groupname;
			icc->grouplist = grouplist;
			icc->groupcount++;

			vfree((yyvsp[(1) - (1)].val));
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 185:

/* Line 1455 of yacc.c  */
#line 1147 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			struct isakmp_cfg_config *icc = &isakmp_cfg_config;

			if (!icc->splitdns_len)
			{
				icc->splitdns_list = racoon_malloc((yyvsp[(1) - (1)].val)->l);
				if(icc->splitdns_list == NULL) {
					yyerror("error allocating splitdns list buffer");
					return -1;
				}
				memcpy(icc->splitdns_list,(yyvsp[(1) - (1)].val)->v,(yyvsp[(1) - (1)].val)->l);
				icc->splitdns_len = (yyvsp[(1) - (1)].val)->l;
			}
			else
			{
				int len = icc->splitdns_len + (yyvsp[(1) - (1)].val)->l + 1;
				icc->splitdns_list = racoon_realloc(icc->splitdns_list,len);
				if(icc->splitdns_list == NULL) {
					yyerror("error allocating splitdns list buffer");
					return -1;
				}
				icc->splitdns_list[icc->splitdns_len] = ',';
				memcpy(icc->splitdns_list + icc->splitdns_len + 1, (yyvsp[(1) - (1)].val)->v, (yyvsp[(1) - (1)].val)->l);
				icc->splitdns_len = len;
			}
			vfree((yyvsp[(1) - (1)].val));
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 189:

/* Line 1455 of yacc.c  */
#line 1191 "cfparse.y"
    {
			lcconf->retry_counter = (yyvsp[(2) - (2)].num);
		}
    break;

  case 191:

/* Line 1455 of yacc.c  */
#line 1196 "cfparse.y"
    {
			lcconf->retry_interval = (yyvsp[(2) - (3)].num) * (yyvsp[(3) - (3)].num);
		}
    break;

  case 193:

/* Line 1455 of yacc.c  */
#line 1201 "cfparse.y"
    {
			lcconf->count_persend = (yyvsp[(2) - (2)].num);
		}
    break;

  case 195:

/* Line 1455 of yacc.c  */
#line 1206 "cfparse.y"
    {
			lcconf->retry_checkph1 = (yyvsp[(2) - (3)].num) * (yyvsp[(3) - (3)].num);
		}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 1211 "cfparse.y"
    {
			lcconf->wait_ph2complete = (yyvsp[(2) - (3)].num) * (yyvsp[(3) - (3)].num);
		}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 1216 "cfparse.y"
    {
#ifdef ENABLE_NATT
        		if (libipsec_opt & LIBIPSEC_OPT_NATT)
				lcconf->natt_ka_interval = (yyvsp[(2) - (3)].num) * (yyvsp[(3) - (3)].num);
			else
                		yyerror("libipsec lacks NAT-T support");
#else
			yyerror("NAT-T support not compiled in.");
#endif
		}
    break;

  case 201:

/* Line 1455 of yacc.c  */
#line 1232 "cfparse.y"
    {
			cur_sainfo = newsainfo();
			if (cur_sainfo == NULL) {
				yyerror("failed to allocate sainfo");
				return -1;
			}
		}
    break;

  case 202:

/* Line 1455 of yacc.c  */
#line 1240 "cfparse.y"
    {
			struct sainfo *check;

			/* default */
			if (cur_sainfo->algs[algclass_ipsec_enc] == 0) {
				yyerror("no encryption algorithm at %s",
					sainfo2str(cur_sainfo));
				return -1;
			}
			if (cur_sainfo->algs[algclass_ipsec_auth] == 0) {
				yyerror("no authentication algorithm at %s",
					sainfo2str(cur_sainfo));
				return -1;
			}
			if (cur_sainfo->algs[algclass_ipsec_comp] == 0) {
				yyerror("no compression algorithm at %s",
					sainfo2str(cur_sainfo));
				return -1;
			}

			/* duplicate check */
			check = getsainfo(cur_sainfo->idsrc,
					  cur_sainfo->iddst,
					  cur_sainfo->id_i,
					  NULL,
					  cur_sainfo->remoteid);

			if (check && ((check->idsrc != SAINFO_ANONYMOUS) &&
				      (cur_sainfo->idsrc != SAINFO_ANONYMOUS))) {
				yyerror("duplicated sainfo: %s",
					sainfo2str(cur_sainfo));
				return -1;
			}

			inssainfo(cur_sainfo);
		}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 1280 "cfparse.y"
    {
			cur_sainfo->idsrc = SAINFO_ANONYMOUS;
			cur_sainfo->iddst = SAINFO_ANONYMOUS;
		}
    break;

  case 205:

/* Line 1455 of yacc.c  */
#line 1285 "cfparse.y"
    {
			cur_sainfo->idsrc = SAINFO_ANONYMOUS;
			cur_sainfo->iddst = SAINFO_CLIENTADDR;
		}
    break;

  case 206:

/* Line 1455 of yacc.c  */
#line 1290 "cfparse.y"
    {
			cur_sainfo->idsrc = SAINFO_ANONYMOUS;
			cur_sainfo->iddst = (yyvsp[(2) - (2)].val);
		}
    break;

  case 207:

/* Line 1455 of yacc.c  */
#line 1295 "cfparse.y"
    {
			cur_sainfo->idsrc = (yyvsp[(1) - (2)].val);
			cur_sainfo->iddst = SAINFO_ANONYMOUS;
		}
    break;

  case 208:

/* Line 1455 of yacc.c  */
#line 1300 "cfparse.y"
    {
			cur_sainfo->idsrc = (yyvsp[(1) - (2)].val);
			cur_sainfo->iddst = SAINFO_CLIENTADDR;
		}
    break;

  case 209:

/* Line 1455 of yacc.c  */
#line 1305 "cfparse.y"
    {
			cur_sainfo->idsrc = (yyvsp[(1) - (2)].val);
			cur_sainfo->iddst = (yyvsp[(2) - (2)].val);
		}
    break;

  case 210:

/* Line 1455 of yacc.c  */
#line 1312 "cfparse.y"
    {
			char portbuf[10];
			struct sockaddr *saddr;

			if (((yyvsp[(5) - (5)].num) == IPPROTO_ICMP || (yyvsp[(5) - (5)].num) == IPPROTO_ICMPV6)
			 && ((yyvsp[(4) - (5)].num) != IPSEC_PORT_ANY || (yyvsp[(4) - (5)].num) != IPSEC_PORT_ANY)) {
				yyerror("port number must be \"any\".");
				return -1;
			}

			snprintf(portbuf, sizeof(portbuf), "%lu", (yyvsp[(4) - (5)].num));
			saddr = str2saddr((yyvsp[(2) - (5)].val)->v, portbuf);
			vfree((yyvsp[(2) - (5)].val));
			if (saddr == NULL)
				return -1;

			switch (saddr->sa_family) {
			case AF_INET:
				if ((yyvsp[(5) - (5)].num) == IPPROTO_ICMPV6) {
					yyerror("upper layer protocol mismatched.\n");
					racoon_free(saddr);
					return -1;
				}
				(yyval.val) = ipsecdoi_sockaddr2id(saddr,
										  (yyvsp[(3) - (5)].num) == ~0 ? (sizeof(struct in_addr) << 3): (yyvsp[(3) - (5)].num),
										  (yyvsp[(5) - (5)].num));
				break;
#ifdef INET6
			case AF_INET6:
				if ((yyvsp[(5) - (5)].num) == IPPROTO_ICMP) {
					yyerror("upper layer protocol mismatched.\n");
					racoon_free(saddr);
					return -1;
				}
				(yyval.val) = ipsecdoi_sockaddr2id(saddr, 
										  (yyvsp[(3) - (5)].num) == ~0 ? (sizeof(struct in6_addr) << 3): (yyvsp[(3) - (5)].num),
										  (yyvsp[(5) - (5)].num));
				break;
#endif
			default:
				yyerror("invalid family: %d", saddr->sa_family);
				(yyval.val) = NULL;
				break;
			}
			racoon_free(saddr);
			if ((yyval.val) == NULL)
				return -1;
		}
    break;

  case 211:

/* Line 1455 of yacc.c  */
#line 1361 "cfparse.y"
    {
			char portbuf[10];
			struct sockaddr *laddr = NULL, *haddr = NULL;
			char *cur = NULL;

			if (((yyvsp[(6) - (6)].num) == IPPROTO_ICMP || (yyvsp[(6) - (6)].num) == IPPROTO_ICMPV6)
			 && ((yyvsp[(5) - (6)].num) != IPSEC_PORT_ANY || (yyvsp[(5) - (6)].num) != IPSEC_PORT_ANY)) {
				yyerror("port number must be \"any\".");
				return -1;
			}

			snprintf(portbuf, sizeof(portbuf), "%lu", (yyvsp[(5) - (6)].num));
			
			laddr = str2saddr((yyvsp[(2) - (6)].val)->v, portbuf);
			if (laddr == NULL) {
			    return -1;
			}
			vfree((yyvsp[(2) - (6)].val));
			haddr = str2saddr((yyvsp[(3) - (6)].val)->v, portbuf);
			if (haddr == NULL) {
			    racoon_free(laddr);
			    return -1;
			}
			vfree((yyvsp[(3) - (6)].val));

			switch (laddr->sa_family) {
			case AF_INET:
				if ((yyvsp[(6) - (6)].num) == IPPROTO_ICMPV6) {
				    yyerror("upper layer protocol mismatched.\n");
				    if (laddr)
					racoon_free(laddr);
				    if (haddr)
					racoon_free(haddr);
				    return -1;
				}
                                (yyval.val) = ipsecdoi_sockrange2id(laddr, haddr, 
							   (yyvsp[(6) - (6)].num));
				break;
#ifdef INET6
			case AF_INET6:
				if ((yyvsp[(6) - (6)].num) == IPPROTO_ICMP) {
					yyerror("upper layer protocol mismatched.\n");
					if (laddr)
					    racoon_free(laddr);
					if (haddr)
					    racoon_free(haddr);
					return -1;
				}
				(yyval.val) = ipsecdoi_sockrange2id(laddr, haddr, 
							       (yyvsp[(6) - (6)].num));
				break;
#endif
			default:
				yyerror("invalid family: %d", laddr->sa_family);
				(yyval.val) = NULL;
				break;
			}
			if (laddr)
			    racoon_free(laddr);
			if (haddr)
			    racoon_free(haddr);
			if ((yyval.val) == NULL)
				return -1;
		}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 1426 "cfparse.y"
    {
			struct ipsecdoi_id_b *id_b;

			if ((yyvsp[(1) - (2)].num) == IDTYPE_ASN1DN) {
				yyerror("id type forbidden: %d", (yyvsp[(1) - (2)].num));
				(yyval.val) = NULL;
				return -1;
			}

			(yyvsp[(2) - (2)].val)->l--;

			(yyval.val) = vmalloc(sizeof(*id_b) + (yyvsp[(2) - (2)].val)->l);
			if ((yyval.val) == NULL) {
				yyerror("failed to allocate identifier");
				return -1;
			}

			id_b = (struct ipsecdoi_id_b *)(yyval.val)->v;
			id_b->type = idtype2doi((yyvsp[(1) - (2)].num));

			id_b->proto_id = 0;
			id_b->port = 0;

			memcpy((yyval.val)->v + sizeof(*id_b), (yyvsp[(2) - (2)].val)->v, (yyvsp[(2) - (2)].val)->l);
		}
    break;

  case 213:

/* Line 1455 of yacc.c  */
#line 1454 "cfparse.y"
    {
			cur_sainfo->id_i = NULL;
		}
    break;

  case 214:

/* Line 1455 of yacc.c  */
#line 1458 "cfparse.y"
    {
			struct ipsecdoi_id_b *id_b;
			vchar_t *idv;

			if (set_identifier(&idv, (yyvsp[(2) - (3)].num), (yyvsp[(3) - (3)].val)) != 0) {
				yyerror("failed to set identifer.\n");
				return -1;
			}
			cur_sainfo->id_i = vmalloc(sizeof(*id_b) + idv->l);
			if (cur_sainfo->id_i == NULL) {
				yyerror("failed to allocate identifier");
				return -1;
			}

			id_b = (struct ipsecdoi_id_b *)cur_sainfo->id_i->v;
			id_b->type = idtype2doi((yyvsp[(2) - (3)].num));

			id_b->proto_id = 0;
			id_b->port = 0;

			memcpy(cur_sainfo->id_i->v + sizeof(*id_b),
			       idv->v, idv->l);
			vfree(idv);
		}
    break;

  case 215:

/* Line 1455 of yacc.c  */
#line 1483 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			if ((cur_sainfo->group = vdup((yyvsp[(2) - (2)].val))) == NULL) {
				yyerror("failed to set sainfo xauth group.\n");
				return -1;
			}
#else
			yyerror("racoon not configured with --enable-hybrid");
			return -1;
#endif
 		}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 1501 "cfparse.y"
    {
			cur_sainfo->pfs_group = (yyvsp[(2) - (2)].num);
		}
    break;

  case 220:

/* Line 1455 of yacc.c  */
#line 1506 "cfparse.y"
    {
			cur_sainfo->remoteid = (yyvsp[(2) - (2)].num);
		}
    break;

  case 222:

/* Line 1455 of yacc.c  */
#line 1511 "cfparse.y"
    {
			cur_sainfo->lifetime = (yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num);
		}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 1516 "cfparse.y"
    {
#if 1
			yyerror("byte lifetime support is deprecated");
			return -1;
#else
			cur_sainfo->lifebyte = fix_lifebyte((yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num));
			if (cur_sainfo->lifebyte == 0)
				return -1;
#endif
		}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 1527 "cfparse.y"
    {
			cur_algclass = (yyvsp[(1) - (1)].num);
		}
    break;

  case 228:

/* Line 1455 of yacc.c  */
#line 1535 "cfparse.y"
    {
			inssainfoalg(&cur_sainfo->algs[cur_algclass], (yyvsp[(1) - (1)].alg));
		}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 1539 "cfparse.y"
    {
			inssainfoalg(&cur_sainfo->algs[cur_algclass], (yyvsp[(1) - (1)].alg));
		}
    break;

  case 231:

/* Line 1455 of yacc.c  */
#line 1546 "cfparse.y"
    {
			int defklen;

			(yyval.alg) = newsainfoalg();
			if ((yyval.alg) == NULL) {
				yyerror("failed to get algorithm allocation");
				return -1;
			}

			(yyval.alg)->alg = algtype2doi(cur_algclass, (yyvsp[(1) - (2)].num));
			if ((yyval.alg)->alg == -1) {
				yyerror("algorithm mismatched");
				racoon_free((yyval.alg));
				(yyval.alg) = NULL;
				return -1;
			}

			defklen = default_keylen(cur_algclass, (yyvsp[(1) - (2)].num));
			if (defklen == 0) {
				if ((yyvsp[(2) - (2)].num)) {
					yyerror("keylen not allowed");
					racoon_free((yyval.alg));
					(yyval.alg) = NULL;
					return -1;
				}
			} else {
				if ((yyvsp[(2) - (2)].num) && check_keylen(cur_algclass, (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].num)) < 0) {
					yyerror("invalid keylen %d", (yyvsp[(2) - (2)].num));
					racoon_free((yyval.alg));
					(yyval.alg) = NULL;
					return -1;
				}
			}

			if ((yyvsp[(2) - (2)].num))
				(yyval.alg)->encklen = (yyvsp[(2) - (2)].num);
			else
				(yyval.alg)->encklen = defklen;

			/* check if it's supported algorithm by kernel */
			if (!(cur_algclass == algclass_ipsec_auth && (yyvsp[(1) - (2)].num) == algtype_non_auth)
			 && pk_checkalg(cur_algclass, (yyvsp[(1) - (2)].num), (yyval.alg)->encklen)) {
				int a = algclass2doi(cur_algclass);
				int b = algtype2doi(cur_algclass, (yyvsp[(1) - (2)].num));
				if (a == IPSECDOI_ATTR_AUTH)
					a = IPSECDOI_PROTO_IPSEC_AH;
				yyerror("algorithm %s not supported by the kernel (missing module?)",
					s_ipsecdoi_trns(a, b));
				racoon_free((yyval.alg));
				(yyval.alg) = NULL;
				return -1;
			}
		}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 1601 "cfparse.y"
    { (yyval.num) = ~0; }
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 1602 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 234:

/* Line 1455 of yacc.c  */
#line 1605 "cfparse.y"
    { (yyval.num) = IPSEC_PORT_ANY; }
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 1606 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 236:

/* Line 1455 of yacc.c  */
#line 1607 "cfparse.y"
    { (yyval.num) = IPSEC_PORT_ANY; }
    break;

  case 237:

/* Line 1455 of yacc.c  */
#line 1610 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 238:

/* Line 1455 of yacc.c  */
#line 1611 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 239:

/* Line 1455 of yacc.c  */
#line 1612 "cfparse.y"
    { (yyval.num) = IPSEC_ULPROTO_ANY; }
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 1615 "cfparse.y"
    { (yyval.num) = 0; }
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 1616 "cfparse.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 242:

/* Line 1455 of yacc.c  */
#line 1622 "cfparse.y"
    {
			struct remoteconf *from, *new;

			if (getrmconf_by_name((yyvsp[(2) - (4)].val)->v) != NULL) {
				yyerror("named remoteconf \"%s\" already exists.");
				return -1;
			}

			from = getrmconf_by_name((yyvsp[(4) - (4)].val)->v);
			if (from == NULL) {
				yyerror("named parent remoteconf \"%s\" does not exist.",
					(yyvsp[(4) - (4)].val)->v);
				return -1;
			}

			new = duprmconf_shallow(from);
			if (new == NULL) {
				yyerror("failed to duplicate remoteconf from \"%s\".",
					(yyvsp[(4) - (4)].val)->v);
				return -1;
			}

			new->name = racoon_strdup((yyvsp[(2) - (4)].val)->v);
			cur_rmconf = new;

			vfree((yyvsp[(2) - (4)].val));
			vfree((yyvsp[(4) - (4)].val));
		}
    break;

  case 244:

/* Line 1455 of yacc.c  */
#line 1652 "cfparse.y"
    {
			struct remoteconf *new;

			if (getrmconf_by_name((yyvsp[(2) - (2)].val)->v) != NULL) {
				yyerror("Named remoteconf \"%s\" already exists.");
				return -1;
			}

			new = newrmconf();
			if (new == NULL) {
				yyerror("failed to get new remoteconf.");
				return -1;
			}
			new->name = racoon_strdup((yyvsp[(2) - (2)].val)->v);
			cur_rmconf = new;

			vfree((yyvsp[(2) - (2)].val));
		}
    break;

  case 246:

/* Line 1455 of yacc.c  */
#line 1672 "cfparse.y"
    {
			struct remoteconf *from, *new;

			from = getrmconf((yyvsp[(4) - (4)].saddr), GETRMCONF_F_NO_ANONYMOUS);
			if (from == NULL) {
				yyerror("failed to get remoteconf for %s.",
					saddr2str((yyvsp[(4) - (4)].saddr)));
				return -1;
			}

			new = duprmconf_shallow(from);
			if (new == NULL) {
				yyerror("failed to duplicate remoteconf from %s.",
					saddr2str((yyvsp[(4) - (4)].saddr)));
				return -1;
			}

			racoon_free((yyvsp[(4) - (4)].saddr));
			new->remote = (yyvsp[(2) - (4)].saddr);
			cur_rmconf = new;
		}
    break;

  case 248:

/* Line 1455 of yacc.c  */
#line 1695 "cfparse.y"
    {
			struct remoteconf *new;

			new = newrmconf();
			if (new == NULL) {
				yyerror("failed to get new remoteconf.");
				return -1;
			}

			new->remote = (yyvsp[(2) - (2)].saddr);
			cur_rmconf = new;
		}
    break;

  case 250:

/* Line 1455 of yacc.c  */
#line 1712 "cfparse.y"
    {
			/* check a exchange mode */
			if (cur_rmconf->etypes == NULL) {
				yyerror("no exchange mode specified.\n");
				return -1;
			}

			if (cur_rmconf->idvtype == IDTYPE_UNDEFINED)
				cur_rmconf->idvtype = IDTYPE_ADDRESS;

			if (cur_rmconf->idvtype == IDTYPE_ASN1DN) {
				if (cur_rmconf->mycertfile) {
					if (cur_rmconf->idv)
						yywarn("Both CERT and ASN1 ID "
						       "are set. Hope this is OK.\n");
					/* TODO: Preparse the DN here */
				} else if (cur_rmconf->idv) {
					/* OK, using asn1dn without X.509. */
				} else {
					yyerror("ASN1 ID not specified "
						"and no CERT defined!\n");
					return -1;
				}
			}

			if (duprmconf_finish(cur_rmconf))
				return -1;

#if 0
			/* this pointer copy will never happen, because duprmconf_shallow
			 * already copied all pointers.
			 */
			if (cur_rmconf->spspec == NULL &&
			    cur_rmconf->inherited_from != NULL) {
				cur_rmconf->spspec = cur_rmconf->inherited_from->spspec;
			}
#endif
			if (set_isakmp_proposal(cur_rmconf) != 0)
				return -1;

			/* DH group settting if aggressive mode is there. */
			if (check_etypeok(cur_rmconf, (void*) ISAKMP_ETYPE_AGG)) {
				struct isakmpsa *p;
				int b = 0;

				/* DH group */
				for (p = cur_rmconf->proposal; p; p = p->next) {
					if (b == 0 || (b && b == p->dh_group)) {
						b = p->dh_group;
						continue;
					}
					yyerror("DH group must be equal "
						"in all proposals "
						"when aggressive mode is "
						"used.\n");
					return -1;
				}
				cur_rmconf->dh_group = b;

				if (cur_rmconf->dh_group == 0) {
					yyerror("DH group must be set in the proposal.\n");
					return -1;
				}

				/* DH group settting if PFS is required. */
				if (oakley_setdhgroup(cur_rmconf->dh_group,
						&cur_rmconf->dhgrp) < 0) {
					yyerror("failed to set DH value.\n");
					return -1;
				}
			}

			insrmconf(cur_rmconf);
		}
    break;

  case 251:

/* Line 1455 of yacc.c  */
#line 1789 "cfparse.y"
    {
			(yyval.saddr) = newsaddr(sizeof(struct sockaddr));
			(yyval.saddr)->sa_family = AF_UNSPEC;
			((struct sockaddr_in *)(yyval.saddr))->sin_port = htons((yyvsp[(2) - (2)].num));
		}
    break;

  case 252:

/* Line 1455 of yacc.c  */
#line 1795 "cfparse.y"
    {
			(yyval.saddr) = (yyvsp[(1) - (1)].saddr);
			if ((yyval.saddr) == NULL) {
				yyerror("failed to allocate sockaddr");
				return -1;
			}
		}
    break;

  case 255:

/* Line 1455 of yacc.c  */
#line 1809 "cfparse.y"
    {
			if (cur_rmconf->remote != NULL) {
				yyerror("remote_address already specified");
				return -1;
			}
			cur_rmconf->remote = (yyvsp[(2) - (2)].saddr);
		}
    break;

  case 257:

/* Line 1455 of yacc.c  */
#line 1818 "cfparse.y"
    {
			cur_rmconf->etypes = NULL;
		}
    break;

  case 259:

/* Line 1455 of yacc.c  */
#line 1822 "cfparse.y"
    { cur_rmconf->doitype = (yyvsp[(2) - (2)].num); }
    break;

  case 261:

/* Line 1455 of yacc.c  */
#line 1823 "cfparse.y"
    { cur_rmconf->sittype = (yyvsp[(2) - (2)].num); }
    break;

  case 264:

/* Line 1455 of yacc.c  */
#line 1826 "cfparse.y"
    {
			yywarn("This directive without certtype will be removed!\n");
			yywarn("Please use 'peers_certfile x509 \"%s\";' instead\n", (yyvsp[(2) - (2)].val)->v);

			if (cur_rmconf->peerscert != NULL) {
				yyerror("peers_certfile already defined\n");
				return -1;
			}

			if (load_x509((yyvsp[(2) - (2)].val)->v, &cur_rmconf->peerscertfile,
				      &cur_rmconf->peerscert)) {
				yyerror("failed to load certificate \"%s\"\n",
					(yyvsp[(2) - (2)].val)->v);
				return -1;
			}

			vfree((yyvsp[(2) - (2)].val));
		}
    break;

  case 266:

/* Line 1455 of yacc.c  */
#line 1846 "cfparse.y"
    {
			if (cur_rmconf->peerscert != NULL) {
				yyerror("peers_certfile already defined\n");
				return -1;
			}

			if (load_x509((yyvsp[(3) - (3)].val)->v, &cur_rmconf->peerscertfile,
				      &cur_rmconf->peerscert)) {
				yyerror("failed to load certificate \"%s\"\n",
					(yyvsp[(3) - (3)].val)->v);
				return -1;
			}

			vfree((yyvsp[(3) - (3)].val));
		}
    break;

  case 268:

/* Line 1455 of yacc.c  */
#line 1863 "cfparse.y"
    {
			char path[MAXPATHLEN];
			int ret = 0;

			if (cur_rmconf->peerscert != NULL) {
				yyerror("peers_certfile already defined\n");
				return -1;
			}

			cur_rmconf->peerscert = vmalloc(1);
			if (cur_rmconf->peerscert == NULL) {
				yyerror("failed to allocate peerscert");
				return -1;
			}
			cur_rmconf->peerscert->v[0] = ISAKMP_CERT_PLAINRSA;

			getpathname(path, sizeof(path),
				    LC_PATHTYPE_CERT, (yyvsp[(3) - (3)].val)->v);
			if (rsa_parse_file(cur_rmconf->rsa_public, path,
					   RSA_TYPE_PUBLIC)) {
				yyerror("Couldn't parse keyfile.\n", path);
				return -1;
			}
			plog(LLV_DEBUG, LOCATION, NULL,
			     "Public PlainRSA keyfile parsed: %s\n", path);

			vfree((yyvsp[(3) - (3)].val));
		}
    break;

  case 270:

/* Line 1455 of yacc.c  */
#line 1893 "cfparse.y"
    {
			if (cur_rmconf->peerscert != NULL) {
				yyerror("peers_certfile already defined\n");
				return -1;
			}
			cur_rmconf->peerscert = vmalloc(1);
			if (cur_rmconf->peerscert == NULL) {
				yyerror("failed to allocate peerscert");
				return -1;
			}
			cur_rmconf->peerscert->v[0] = ISAKMP_CERT_DNS;
		}
    break;

  case 272:

/* Line 1455 of yacc.c  */
#line 1907 "cfparse.y"
    {
			if (cur_rmconf->cacert != NULL) {
				yyerror("ca_type already defined\n");
				return -1;
			}

			if (load_x509((yyvsp[(3) - (3)].val)->v, &cur_rmconf->cacertfile,
				      &cur_rmconf->cacert)) {
				yyerror("failed to load certificate \"%s\"\n",
					(yyvsp[(3) - (3)].val)->v);
				return -1;
			}

			vfree((yyvsp[(3) - (3)].val));
		}
    break;

  case 274:

/* Line 1455 of yacc.c  */
#line 1923 "cfparse.y"
    { cur_rmconf->verify_cert = (yyvsp[(2) - (2)].num); }
    break;

  case 276:

/* Line 1455 of yacc.c  */
#line 1924 "cfparse.y"
    { cur_rmconf->send_cert = (yyvsp[(2) - (2)].num); }
    break;

  case 278:

/* Line 1455 of yacc.c  */
#line 1925 "cfparse.y"
    { cur_rmconf->send_cr = (yyvsp[(2) - (2)].num); }
    break;

  case 280:

/* Line 1455 of yacc.c  */
#line 1926 "cfparse.y"
    { cur_rmconf->match_empty_cr = (yyvsp[(2) - (2)].num); }
    break;

  case 282:

/* Line 1455 of yacc.c  */
#line 1928 "cfparse.y"
    {
			if (set_identifier(&cur_rmconf->idv, (yyvsp[(2) - (3)].num), (yyvsp[(3) - (3)].val)) != 0) {
				yyerror("failed to set identifer.\n");
				return -1;
			}
			cur_rmconf->idvtype = (yyvsp[(2) - (3)].num);
		}
    break;

  case 284:

/* Line 1455 of yacc.c  */
#line 1937 "cfparse.y"
    {
			if (set_identifier_qual(&cur_rmconf->idv, (yyvsp[(2) - (4)].num), (yyvsp[(4) - (4)].val), (yyvsp[(3) - (4)].num)) != 0) {
				yyerror("failed to set identifer.\n");
				return -1;
			}
			cur_rmconf->idvtype = (yyvsp[(2) - (4)].num);
		}
    break;

  case 286:

/* Line 1455 of yacc.c  */
#line 1946 "cfparse.y"
    {
#ifdef ENABLE_HYBRID
			/* formerly identifier type login */
			if (xauth_rmconf_used(&cur_rmconf->xauth) == -1) {
				yyerror("failed to allocate xauth state\n");
				return -1;
			}
			if ((cur_rmconf->xauth->login = vdup((yyvsp[(2) - (2)].val))) == NULL) {
				yyerror("failed to set identifer.\n");
				return -1;
			}
#else
			yyerror("racoon not configured with --enable-hybrid");
#endif
		}
    break;

  case 288:

/* Line 1455 of yacc.c  */
#line 1963 "cfparse.y"
    {
			struct idspec  *id;
			id = newidspec();
			if (id == NULL) {
				yyerror("failed to allocate idspec");
				return -1;
			}
			if (set_identifier(&id->id, (yyvsp[(2) - (3)].num), (yyvsp[(3) - (3)].val)) != 0) {
				yyerror("failed to set identifer.\n");
				racoon_free(id);
				return -1;
			}
			id->idtype = (yyvsp[(2) - (3)].num);
			genlist_append (cur_rmconf->idvl_p, id);
		}
    break;

  case 290:

/* Line 1455 of yacc.c  */
#line 1980 "cfparse.y"
    {
			struct idspec  *id;
			id = newidspec();
			if (id == NULL) {
				yyerror("failed to allocate idspec");
				return -1;
			}
			if (set_identifier_qual(&id->id, (yyvsp[(2) - (4)].num), (yyvsp[(4) - (4)].val), (yyvsp[(3) - (4)].num)) != 0) {
				yyerror("failed to set identifer.\n");
				racoon_free(id);
				return -1;
			}
			id->idtype = (yyvsp[(2) - (4)].num);
			genlist_append (cur_rmconf->idvl_p, id);
		}
    break;

  case 292:

/* Line 1455 of yacc.c  */
#line 1996 "cfparse.y"
    { cur_rmconf->verify_identifier = (yyvsp[(2) - (2)].num); }
    break;

  case 294:

/* Line 1455 of yacc.c  */
#line 1997 "cfparse.y"
    { cur_rmconf->nonce_size = (yyvsp[(2) - (2)].num); }
    break;

  case 296:

/* Line 1455 of yacc.c  */
#line 1999 "cfparse.y"
    {
			yyerror("dh_group cannot be defined here.");
			return -1;
		}
    break;

  case 298:

/* Line 1455 of yacc.c  */
#line 2004 "cfparse.y"
    { cur_rmconf->passive = (yyvsp[(2) - (2)].num); }
    break;

  case 300:

/* Line 1455 of yacc.c  */
#line 2005 "cfparse.y"
    { cur_rmconf->remote_access = (yyvsp[(2) - (2)].num); }
    break;

  case 302:

/* Line 1455 of yacc.c  */
#line 2006 "cfparse.y"
    { cur_rmconf->dynamic_site = (yyvsp[(2) - (2)].num); }
    break;

  case 304:

/* Line 1455 of yacc.c  */
#line 2007 "cfparse.y"
    { cur_rmconf->sa_assoc = (yyvsp[(2) - (2)].num); }
    break;

  case 306:

/* Line 1455 of yacc.c  */
#line 2008 "cfparse.y"
    { cur_rmconf->use_natt_addr = (yyvsp[(2) - (2)].num); }
    break;

  case 308:

/* Line 1455 of yacc.c  */
#line 2009 "cfparse.y"
    { cur_rmconf->ike_frag = (yyvsp[(2) - (2)].num); }
    break;

  case 310:

/* Line 1455 of yacc.c  */
#line 2010 "cfparse.y"
    { cur_rmconf->ike_frag = ISAKMP_FRAG_FORCE; }
    break;

  case 312:

/* Line 1455 of yacc.c  */
#line 2011 "cfparse.y"
    { 
#ifdef SADB_X_EXT_NAT_T_FRAG
        		if (libipsec_opt & LIBIPSEC_OPT_FRAG)
				cur_rmconf->esp_frag = (yyvsp[(2) - (2)].num); 
			else
                		yywarn("libipsec lacks IKE frag support");
#else
			yywarn("Your kernel does not support esp_frag");
#endif
		}
    break;

  case 314:

/* Line 1455 of yacc.c  */
#line 2021 "cfparse.y"
    { 
			if (cur_rmconf->script[SCRIPT_PHASE1_UP] != NULL)
				vfree(cur_rmconf->script[SCRIPT_PHASE1_UP]);

			cur_rmconf->script[SCRIPT_PHASE1_UP] = 
			    script_path_add(vdup((yyvsp[(2) - (3)].val)));
		}
    break;

  case 316:

/* Line 1455 of yacc.c  */
#line 2028 "cfparse.y"
    { 
			if (cur_rmconf->script[SCRIPT_PHASE1_DOWN] != NULL)
				vfree(cur_rmconf->script[SCRIPT_PHASE1_DOWN]);

			cur_rmconf->script[SCRIPT_PHASE1_DOWN] = 
			    script_path_add(vdup((yyvsp[(2) - (3)].val)));
		}
    break;

  case 318:

/* Line 1455 of yacc.c  */
#line 2035 "cfparse.y"
    { 
			if (cur_rmconf->script[SCRIPT_PHASE1_DEAD] != NULL)
				vfree(cur_rmconf->script[SCRIPT_PHASE1_DEAD]);

			cur_rmconf->script[SCRIPT_PHASE1_DEAD] = 
			    script_path_add(vdup((yyvsp[(2) - (3)].val)));
		}
    break;

  case 320:

/* Line 1455 of yacc.c  */
#line 2042 "cfparse.y"
    { cur_rmconf->mode_cfg = (yyvsp[(2) - (2)].num); }
    break;

  case 322:

/* Line 1455 of yacc.c  */
#line 2043 "cfparse.y"
    {
			cur_rmconf->weak_phase1_check = (yyvsp[(2) - (2)].num);
		}
    break;

  case 324:

/* Line 1455 of yacc.c  */
#line 2046 "cfparse.y"
    { cur_rmconf->gen_policy = (yyvsp[(2) - (2)].num); }
    break;

  case 326:

/* Line 1455 of yacc.c  */
#line 2047 "cfparse.y"
    { cur_rmconf->gen_policy = (yyvsp[(2) - (2)].num); }
    break;

  case 328:

/* Line 1455 of yacc.c  */
#line 2048 "cfparse.y"
    { cur_rmconf->support_proxy = (yyvsp[(2) - (2)].num); }
    break;

  case 330:

/* Line 1455 of yacc.c  */
#line 2049 "cfparse.y"
    { cur_rmconf->ini_contact = (yyvsp[(2) - (2)].num); }
    break;

  case 332:

/* Line 1455 of yacc.c  */
#line 2051 "cfparse.y"
    {
#ifdef ENABLE_NATT
        		if (libipsec_opt & LIBIPSEC_OPT_NATT)
				cur_rmconf->nat_traversal = (yyvsp[(2) - (2)].num);
			else
                		yyerror("libipsec lacks NAT-T support");
#else
			yyerror("NAT-T support not compiled in.");
#endif
		}
    break;

  case 334:

/* Line 1455 of yacc.c  */
#line 2062 "cfparse.y"
    {
#ifdef ENABLE_NATT
			if (libipsec_opt & LIBIPSEC_OPT_NATT)
				cur_rmconf->nat_traversal = NATT_FORCE;
			else
                		yyerror("libipsec lacks NAT-T support");
#else
			yyerror("NAT-T support not compiled in.");
#endif
		}
    break;

  case 336:

/* Line 1455 of yacc.c  */
#line 2073 "cfparse.y"
    {
#ifdef ENABLE_DPD
			cur_rmconf->dpd = (yyvsp[(2) - (2)].num);
#else
			yyerror("DPD support not compiled in.");
#endif
		}
    break;

  case 338:

/* Line 1455 of yacc.c  */
#line 2081 "cfparse.y"
    {
#ifdef ENABLE_DPD
			cur_rmconf->dpd_interval = (yyvsp[(2) - (2)].num);
#else
			yyerror("DPD support not compiled in.");
#endif
		}
    break;

  case 340:

/* Line 1455 of yacc.c  */
#line 2090 "cfparse.y"
    {
#ifdef ENABLE_DPD
			cur_rmconf->dpd_retry = (yyvsp[(2) - (2)].num);
#else
			yyerror("DPD support not compiled in.");
#endif
		}
    break;

  case 342:

/* Line 1455 of yacc.c  */
#line 2099 "cfparse.y"
    {
#ifdef ENABLE_DPD
			cur_rmconf->dpd_maxfails = (yyvsp[(2) - (2)].num);
#else
			yyerror("DPD support not compiled in.");
#endif
		}
    break;

  case 344:

/* Line 1455 of yacc.c  */
#line 2107 "cfparse.y"
    { cur_rmconf->rekey = (yyvsp[(2) - (2)].num); }
    break;

  case 346:

/* Line 1455 of yacc.c  */
#line 2108 "cfparse.y"
    { cur_rmconf->rekey = REKEY_FORCE; }
    break;

  case 348:

/* Line 1455 of yacc.c  */
#line 2110 "cfparse.y"
    {
			cur_rmconf->ph1id = (yyvsp[(2) - (2)].num);
		}
    break;

  case 350:

/* Line 1455 of yacc.c  */
#line 2115 "cfparse.y"
    {
			cur_rmconf->lifetime = (yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num);
		}
    break;

  case 352:

/* Line 1455 of yacc.c  */
#line 2119 "cfparse.y"
    { cur_rmconf->pcheck_level = (yyvsp[(2) - (2)].num); }
    break;

  case 354:

/* Line 1455 of yacc.c  */
#line 2121 "cfparse.y"
    {
#if 1
			yyerror("byte lifetime support is deprecated in Phase1");
			return -1;
#else
			yywarn("the lifetime of bytes in phase 1 "
				"will be ignored at the moment.");
			cur_rmconf->lifebyte = fix_lifebyte((yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num));
			if (cur_rmconf->lifebyte == 0)
				return -1;
#endif
		}
    break;

  case 356:

/* Line 1455 of yacc.c  */
#line 2135 "cfparse.y"
    {
			struct secprotospec *spspec;

			spspec = newspspec();
			if (spspec == NULL)
				return -1;
			insspspec(cur_rmconf, spspec);
		}
    break;

  case 359:

/* Line 1455 of yacc.c  */
#line 2148 "cfparse.y"
    {
			struct etypes *new;
			new = racoon_malloc(sizeof(struct etypes));
			if (new == NULL) {
				yyerror("failed to allocate etypes");
				return -1;
			}
			new->type = (yyvsp[(2) - (2)].num);
			new->next = NULL;
			if (cur_rmconf->etypes == NULL)
				cur_rmconf->etypes = new;
			else {
				struct etypes *p;
				for (p = cur_rmconf->etypes;
				     p->next != NULL;
				     p = p->next)
					;
				p->next = new;
			}
		}
    break;

  case 360:

/* Line 1455 of yacc.c  */
#line 2171 "cfparse.y"
    {
			if (cur_rmconf->mycert != NULL) {
				yyerror("certificate_type already defined\n");
				return -1;
			}

			if (load_x509((yyvsp[(2) - (3)].val)->v, &cur_rmconf->mycertfile,
				      &cur_rmconf->mycert)) {
				yyerror("failed to load certificate \"%s\"\n",
					(yyvsp[(2) - (3)].val)->v);
				return -1;
			}

			cur_rmconf->myprivfile = racoon_strdup((yyvsp[(3) - (3)].val)->v);
			STRDUP_FATAL(cur_rmconf->myprivfile);

			vfree((yyvsp[(2) - (3)].val));
			vfree((yyvsp[(3) - (3)].val));
		}
    break;

  case 362:

/* Line 1455 of yacc.c  */
#line 2192 "cfparse.y"
    {
			char path[MAXPATHLEN];
			int ret = 0;

			if (cur_rmconf->mycert != NULL) {
				yyerror("certificate_type already defined\n");
				return -1;
			}

			cur_rmconf->mycert = vmalloc(1);
			if (cur_rmconf->mycert == NULL) {
				yyerror("failed to allocate mycert");
				return -1;
			}
			cur_rmconf->mycert->v[0] = ISAKMP_CERT_PLAINRSA;

			getpathname(path, sizeof(path),
				    LC_PATHTYPE_CERT, (yyvsp[(2) - (2)].val)->v);
			cur_rmconf->send_cr = FALSE;
			cur_rmconf->send_cert = FALSE;
			cur_rmconf->verify_cert = FALSE;
			if (rsa_parse_file(cur_rmconf->rsa_private, path,
					   RSA_TYPE_PRIVATE)) {
				yyerror("Couldn't parse keyfile.\n", path);
				return -1;
			}
			plog(LLV_DEBUG, LOCATION, NULL,
			     "Private PlainRSA keyfile parsed: %s\n", path);
			vfree((yyvsp[(2) - (2)].val));
		}
    break;

  case 364:

/* Line 1455 of yacc.c  */
#line 2226 "cfparse.y"
    {
			(yyval.num) = algtype2doi(algclass_isakmp_dh, (yyvsp[(1) - (1)].num));
			if ((yyval.num) == -1) {
				yyerror("must be DH group");
				return -1;
			}
		}
    break;

  case 365:

/* Line 1455 of yacc.c  */
#line 2234 "cfparse.y"
    {
			if (ARRAYLEN(num2dhgroup) > (yyvsp[(1) - (1)].num) && num2dhgroup[(yyvsp[(1) - (1)].num)] != 0) {
				(yyval.num) = num2dhgroup[(yyvsp[(1) - (1)].num)];
			} else {
				yyerror("must be DH group");
				(yyval.num) = 0;
				return -1;
			}
		}
    break;

  case 366:

/* Line 1455 of yacc.c  */
#line 2245 "cfparse.y"
    { (yyval.val) = NULL; }
    break;

  case 367:

/* Line 1455 of yacc.c  */
#line 2246 "cfparse.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); }
    break;

  case 368:

/* Line 1455 of yacc.c  */
#line 2247 "cfparse.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); }
    break;

  case 371:

/* Line 1455 of yacc.c  */
#line 2255 "cfparse.y"
    {
			cur_rmconf->spspec->lifetime = (yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num);
		}
    break;

  case 373:

/* Line 1455 of yacc.c  */
#line 2260 "cfparse.y"
    {
#if 1
			yyerror("byte lifetime support is deprecated");
			return -1;
#else
			cur_rmconf->spspec->lifebyte = fix_lifebyte((yyvsp[(3) - (4)].num) * (yyvsp[(4) - (4)].num));
			if (cur_rmconf->spspec->lifebyte == 0)
				return -1;
#endif
		}
    break;

  case 375:

/* Line 1455 of yacc.c  */
#line 2272 "cfparse.y"
    {
			cur_rmconf->spspec->algclass[algclass_isakmp_dh] = (yyvsp[(2) - (2)].num);
		}
    break;

  case 377:

/* Line 1455 of yacc.c  */
#line 2277 "cfparse.y"
    {
			if (cur_rmconf->spspec->vendorid != VENDORID_GSSAPI) {
				yyerror("wrong Vendor ID for gssapi_id");
				return -1;
			}
			if (cur_rmconf->spspec->gssid != NULL)
				racoon_free(cur_rmconf->spspec->gssid);
			cur_rmconf->spspec->gssid =
			    racoon_strdup((yyvsp[(2) - (2)].val)->v);
			STRDUP_FATAL(cur_rmconf->spspec->gssid);
		}
    break;

  case 379:

/* Line 1455 of yacc.c  */
#line 2290 "cfparse.y"
    {
			int doi;
			int defklen;

			doi = algtype2doi((yyvsp[(1) - (3)].num), (yyvsp[(2) - (3)].num));
			if (doi == -1) {
				yyerror("algorithm mismatched 1");
				return -1;
			}

			switch ((yyvsp[(1) - (3)].num)) {
			case algclass_isakmp_enc:
			/* reject suppressed algorithms */
#ifndef HAVE_OPENSSL_RC5_H
				if ((yyvsp[(2) - (3)].num) == algtype_rc5) {
					yyerror("algorithm %s not supported",
					    s_attr_isakmp_enc(doi));
					return -1;
				}
#endif
#ifndef HAVE_OPENSSL_IDEA_H
				if ((yyvsp[(2) - (3)].num) == algtype_idea) {
					yyerror("algorithm %s not supported",
					    s_attr_isakmp_enc(doi));
					return -1;
				}
#endif

				cur_rmconf->spspec->algclass[algclass_isakmp_enc] = doi;
				defklen = default_keylen((yyvsp[(1) - (3)].num), (yyvsp[(2) - (3)].num));
				if (defklen == 0) {
					if ((yyvsp[(3) - (3)].num)) {
						yyerror("keylen not allowed");
						return -1;
					}
				} else {
					if ((yyvsp[(3) - (3)].num) && check_keylen((yyvsp[(1) - (3)].num), (yyvsp[(2) - (3)].num), (yyvsp[(3) - (3)].num)) < 0) {
						yyerror("invalid keylen %d", (yyvsp[(3) - (3)].num));
						return -1;
					}
				}
				if ((yyvsp[(3) - (3)].num))
					cur_rmconf->spspec->encklen = (yyvsp[(3) - (3)].num);
				else
					cur_rmconf->spspec->encklen = defklen;
				break;
			case algclass_isakmp_hash:
				cur_rmconf->spspec->algclass[algclass_isakmp_hash] = doi;
				break;
			case algclass_isakmp_ameth:
				cur_rmconf->spspec->algclass[algclass_isakmp_ameth] = doi;
				/*
				 * We may have to set the Vendor ID for the
				 * authentication method we're using.
				 */
				switch ((yyvsp[(2) - (3)].num)) {
				case algtype_gssapikrb:
					if (cur_rmconf->spspec->vendorid !=
					    VENDORID_UNKNOWN) {
						yyerror("Vendor ID mismatch "
						    "for auth method");
						return -1;
					}
					/*
					 * For interoperability with Win2k,
					 * we set the Vendor ID to "GSSAPI".
					 */
					cur_rmconf->spspec->vendorid =
					    VENDORID_GSSAPI;
					break;
				case algtype_rsasig:
					if (oakley_get_certtype(cur_rmconf->peerscert) == ISAKMP_CERT_PLAINRSA) {
						if (rsa_list_count(cur_rmconf->rsa_private) == 0) {
							yyerror ("Private PlainRSA key not set. "
								 "Use directive 'certificate_type plainrsa ...'\n");
							return -1;
						}
						if (rsa_list_count(cur_rmconf->rsa_public) == 0) {
							yyerror ("Public PlainRSA keys not set. "
								 "Use directive 'peers_certfile plainrsa ...'\n");
							return -1;
						}
					}
					break;
				default:
					break;
				}
				break;
			default:
				yyerror("algorithm mismatched 2");
				return -1;
			}
		}
    break;

  case 381:

/* Line 1455 of yacc.c  */
#line 2387 "cfparse.y"
    { (yyval.num) = 1; }
    break;

  case 382:

/* Line 1455 of yacc.c  */
#line 2388 "cfparse.y"
    { (yyval.num) = 60; }
    break;

  case 383:

/* Line 1455 of yacc.c  */
#line 2389 "cfparse.y"
    { (yyval.num) = (60 * 60); }
    break;

  case 384:

/* Line 1455 of yacc.c  */
#line 2392 "cfparse.y"
    { (yyval.num) = 1; }
    break;

  case 385:

/* Line 1455 of yacc.c  */
#line 2393 "cfparse.y"
    { (yyval.num) = 1024; }
    break;

  case 386:

/* Line 1455 of yacc.c  */
#line 2394 "cfparse.y"
    { (yyval.num) = (1024 * 1024); }
    break;

  case 387:

/* Line 1455 of yacc.c  */
#line 2395 "cfparse.y"
    { (yyval.num) = (1024 * 1024 * 1024); }
    break;



/* Line 1455 of yacc.c  */
#line 5386 "cfparse.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 2397 "cfparse.y"


static struct secprotospec *
newspspec()
{
	struct secprotospec *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL) {
		yyerror("failed to allocate spproto");
		return NULL;
	}

	new->encklen = 0;	/*XXX*/

	/*
	 * Default to "uknown" vendor -- we will override this
	 * as necessary.  When we send a Vendor ID payload, an
	 * "unknown" will be translated to a KAME/racoon ID.
	 */
	new->vendorid = VENDORID_UNKNOWN;

	return new;
}

/*
 * insert into head of list.
 */
static void
insspspec(rmconf, spspec)
	struct remoteconf *rmconf;
	struct secprotospec *spspec;
{
	if (rmconf->spspec != NULL)
		rmconf->spspec->prev = spspec;
	spspec->next = rmconf->spspec;
	rmconf->spspec = spspec;
}

static struct secprotospec *
dupspspec(spspec)
	struct secprotospec *spspec;
{
	struct secprotospec *new;

	new = newspspec();
	if (new == NULL) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "dupspspec: malloc failed\n");
		return NULL;
	}
	memcpy(new, spspec, sizeof(*new));

	if (spspec->gssid) {
		new->gssid = racoon_strdup(spspec->gssid);
		STRDUP_FATAL(new->gssid);
	}
	if (spspec->remote) {
		new->remote = racoon_malloc(sizeof(*new->remote));
		if (new->remote == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
			    "dupspspec: malloc failed (remote)\n");
			return NULL;
		}
		memcpy(new->remote, spspec->remote, sizeof(*new->remote));
	}

	return new;
}

/*
 * copy the whole list
 */
void
dupspspec_list(dst, src)
	struct remoteconf *dst, *src;
{
	struct secprotospec *p, *new, *last;

	for(p = src->spspec, last = NULL; p; p = p->next, last = new) {
		new = dupspspec(p);
		if (new == NULL)
			exit(1);

		new->prev = last;
		new->next = NULL; /* not necessary but clean */

		if (last)
			last->next = new;
		else /* first element */
			dst->spspec = new;

	}
}

/*
 * delete the whole list
 */
void
flushspspec(rmconf)
	struct remoteconf *rmconf;
{
	struct secprotospec *p;

	while(rmconf->spspec != NULL) {
		p = rmconf->spspec;
		rmconf->spspec = p->next;
		if (p->next != NULL)
			p->next->prev = NULL; /* not necessary but clean */

		if (p->gssid)
			racoon_free(p->gssid);
		if (p->remote)
			racoon_free(p->remote);
		racoon_free(p);
	}
	rmconf->spspec = NULL;
}

/* set final acceptable proposal */
static int
set_isakmp_proposal(rmconf)
	struct remoteconf *rmconf;
{
	struct secprotospec *s;
	int prop_no = 1; 
	int trns_no = 1;
	int32_t types[MAXALGCLASS];

	/* mandatory check */
	if (rmconf->spspec == NULL) {
		yyerror("no remote specification found: %s.\n",
			saddr2str(rmconf->remote));
		return -1;
	}
	for (s = rmconf->spspec; s != NULL; s = s->next) {
		/* XXX need more to check */
		if (s->algclass[algclass_isakmp_enc] == 0) {
			yyerror("encryption algorithm required.");
			return -1;
		}
		if (s->algclass[algclass_isakmp_hash] == 0) {
			yyerror("hash algorithm required.");
			return -1;
		}
		if (s->algclass[algclass_isakmp_dh] == 0) {
			yyerror("DH group required.");
			return -1;
		}
		if (s->algclass[algclass_isakmp_ameth] == 0) {
			yyerror("authentication method required.");
			return -1;
		}
	}

	/* skip to last part */
	for (s = rmconf->spspec; s->next != NULL; s = s->next)
		;

	while (s != NULL) {
		plog(LLV_DEBUG2, LOCATION, NULL,
			"lifetime = %ld\n", (long)
			(s->lifetime ? s->lifetime : rmconf->lifetime));
		plog(LLV_DEBUG2, LOCATION, NULL,
			"lifebyte = %d\n",
			s->lifebyte ? s->lifebyte : rmconf->lifebyte);
		plog(LLV_DEBUG2, LOCATION, NULL,
			"encklen=%d\n", s->encklen);

		memset(types, 0, ARRAYLEN(types));
		types[algclass_isakmp_enc] = s->algclass[algclass_isakmp_enc];
		types[algclass_isakmp_hash] = s->algclass[algclass_isakmp_hash];
		types[algclass_isakmp_dh] = s->algclass[algclass_isakmp_dh];
		types[algclass_isakmp_ameth] =
		    s->algclass[algclass_isakmp_ameth];

		/* expanding spspec */
		clean_tmpalgtype();
		trns_no = expand_isakmpspec(prop_no, trns_no, types,
				algclass_isakmp_enc, algclass_isakmp_ameth + 1,
				s->lifetime ? s->lifetime : rmconf->lifetime,
				s->lifebyte ? s->lifebyte : rmconf->lifebyte,
				s->encklen, s->vendorid, s->gssid,
				rmconf);
		if (trns_no == -1) {
			plog(LLV_ERROR, LOCATION, NULL,
				"failed to expand isakmp proposal.\n");
			return -1;
		}

		s = s->prev;
	}

	if (rmconf->proposal == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"no proposal found.\n");
		return -1;
	}

	return 0;
}

static void
clean_tmpalgtype()
{
	int i;
	for (i = 0; i < MAXALGCLASS; i++)
		tmpalgtype[i] = 0;	/* means algorithm undefined. */
}

static int
expand_isakmpspec(prop_no, trns_no, types,
		class, last, lifetime, lifebyte, encklen, vendorid, gssid,
		rmconf)
	int prop_no, trns_no;
	int *types, class, last;
	time_t lifetime;
	int lifebyte;
	int encklen;
	int vendorid;
	char *gssid;
	struct remoteconf *rmconf;
{
	struct isakmpsa *new;

	/* debugging */
    {
	int j;
	char tb[10];
	plog(LLV_DEBUG2, LOCATION, NULL,
		"p:%d t:%d\n", prop_no, trns_no);
	for (j = class; j < MAXALGCLASS; j++) {
		snprintf(tb, sizeof(tb), "%d", types[j]);
		plog(LLV_DEBUG2, LOCATION, NULL,
			"%s%s%s%s\n",
			s_algtype(j, types[j]),
			types[j] ? "(" : "",
			tb[0] == '0' ? "" : tb,
			types[j] ? ")" : "");
	}
	plog(LLV_DEBUG2, LOCATION, NULL, "\n");
    }

#define TMPALGTYPE2STR(n) \
	s_algtype(algclass_isakmp_##n, types[algclass_isakmp_##n])
		/* check mandatory values */
		if (types[algclass_isakmp_enc] == 0
		 || types[algclass_isakmp_ameth] == 0
		 || types[algclass_isakmp_hash] == 0
		 || types[algclass_isakmp_dh] == 0) {
			yyerror("few definition of algorithm "
				"enc=%s ameth=%s hash=%s dhgroup=%s.\n",
				TMPALGTYPE2STR(enc),
				TMPALGTYPE2STR(ameth),
				TMPALGTYPE2STR(hash),
				TMPALGTYPE2STR(dh));
			return -1;
		}
#undef TMPALGTYPE2STR

	/* set new sa */
	new = newisakmpsa();
	if (new == NULL) {
		yyerror("failed to allocate isakmp sa");
		return -1;
	}
	new->prop_no = prop_no;
	new->trns_no = trns_no++;
	new->lifetime = lifetime;
	new->lifebyte = lifebyte;
	new->enctype = types[algclass_isakmp_enc];
	new->encklen = encklen;
	new->authmethod = types[algclass_isakmp_ameth];
	new->hashtype = types[algclass_isakmp_hash];
	new->dh_group = types[algclass_isakmp_dh];
	new->vendorid = vendorid;
#ifdef HAVE_GSSAPI
	if (new->authmethod == OAKLEY_ATTR_AUTH_METHOD_GSSAPI_KRB) {
		if (gssid != NULL) {
			if ((new->gssid = vmalloc(strlen(gssid))) == NULL) {
				racoon_free(new);
				yyerror("failed to allocate gssid");
				return -1;
			}
			memcpy(new->gssid->v, gssid, new->gssid->l);
			racoon_free(gssid);
		} else {
			/*
			 * Allocate the default ID so that it gets put
			 * into a GSS ID attribute during the Phase 1
			 * exchange.
			 */
			new->gssid = gssapi_get_default_gss_id();
		}
	}
#endif
	insisakmpsa(new, rmconf);

	return trns_no;
}

#if 0
/*
 * fix lifebyte.
 * Must be more than 1024B because its unit is kilobytes.
 * That is defined RFC2407.
 */
static int
fix_lifebyte(t)
	unsigned long t;
{
	if (t < 1024) {
		yyerror("byte size should be more than 1024B.");
		return 0;
	}

	return(t / 1024);
}
#endif

int
cfparse()
{
	int error;

	yyerrorcount = 0;
	yycf_init_buffer();

	if (yycf_switch_buffer(lcconf->racoon_conf) != 0) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "could not read configuration file \"%s\"\n", 
		    lcconf->racoon_conf);
		return -1;
	}

	error = yyparse();
	if (error != 0) {
		if (yyerrorcount) {
			plog(LLV_ERROR, LOCATION, NULL,
				"fatal parse failure (%d errors)\n",
				yyerrorcount);
		} else {
			plog(LLV_ERROR, LOCATION, NULL,
				"fatal parse failure.\n");
		}
		return -1;
	}

	if (error == 0 && yyerrorcount) {
		plog(LLV_ERROR, LOCATION, NULL,
			"parse error is nothing, but yyerrorcount is %d.\n",
				yyerrorcount);
		exit(1);
	}

	yycf_clean_buffer();

	plog(LLV_DEBUG2, LOCATION, NULL, "parse successed.\n");

	return 0;
}

int
cfreparse()
{
	flushph2();
	flushph1();
	flushrmconf();
	flushsainfo();
	clean_tmpalgtype();
	return(cfparse());
}

#ifdef ENABLE_ADMINPORT
static void
adminsock_conf(path, owner, group, mode_dec)
	vchar_t *path;
	vchar_t *owner;
	vchar_t *group;
	int mode_dec;
{
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	mode_t mode = 0;
	uid_t uid;
	gid_t gid;
	int isnum;

	adminsock_path = path->v;

	if (owner == NULL)
		return;

	errno = 0;
	uid = atoi(owner->v);
	isnum = !errno;
	if (((pw = getpwnam(owner->v)) == NULL) && !isnum)
		yyerror("User \"%s\" does not exist", owner->v);

	if (pw)
		adminsock_owner = pw->pw_uid;
	else
		adminsock_owner = uid;

	if (group == NULL)
		return;

	errno = 0;
	gid = atoi(group->v);
	isnum = !errno;
	if (((gr = getgrnam(group->v)) == NULL) && !isnum)
		yyerror("Group \"%s\" does not exist", group->v);

	if (gr)
		adminsock_group = gr->gr_gid;
	else
		adminsock_group = gid;

	if (mode_dec == -1)
		return;

	if (mode_dec > 777)
		yyerror("Mode 0%03o is invalid", mode_dec);
	if (mode_dec >= 400) { mode += 0400; mode_dec -= 400; }
	if (mode_dec >= 200) { mode += 0200; mode_dec -= 200; }
	if (mode_dec >= 100) { mode += 0200; mode_dec -= 100; }

	if (mode_dec > 77)
		yyerror("Mode 0%03o is invalid", mode_dec);
	if (mode_dec >= 40) { mode += 040; mode_dec -= 40; }
	if (mode_dec >= 20) { mode += 020; mode_dec -= 20; }
	if (mode_dec >= 10) { mode += 020; mode_dec -= 10; }

	if (mode_dec > 7)
		yyerror("Mode 0%03o is invalid", mode_dec);
	if (mode_dec >= 4) { mode += 04; mode_dec -= 4; }
	if (mode_dec >= 2) { mode += 02; mode_dec -= 2; }
	if (mode_dec >= 1) { mode += 02; mode_dec -= 1; }
	
	adminsock_mode = mode;

	return;
}
#endif

