
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

/* Substitute the variable and function names.  */
#define yyparse         pcap_parse
#define yylex           pcap_lex
#define yyerror         pcap_error
#define yylval          pcap_lval
#define yychar          pcap_char
#define yydebug         pcap_debug
#define yynerrs         pcap_nerrs


/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "grammar.y"

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/libpcap/grammar.y,v 1.78 2002/12/06 00:01:34 hannes Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else /* WIN32 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#endif /* WIN32 */

#include <stdlib.h>

#ifndef WIN32
#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <netinet/in.h>
#endif /* WIN32 */

#include <stdio.h>

#include "pcap-int.h"

#include "gencode.h"
#include <pcap-namedb.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define QSET(q, p, d, a) (q).proto = (p),\
			 (q).dir = (d),\
			 (q).addr = (a)

int n_errors = 0;

static struct qual qerr = { Q_UNDEF, Q_UNDEF, Q_UNDEF, Q_UNDEF };

static void
yyerror(char *msg)
{
	++n_errors;
	bpf_error("%s", msg);
	/* NOTREACHED */
}

#ifndef YYBISON
int yyparse(void);

int
pcap_parse()
{
	return (yyparse());
}
#endif



/* Line 189 of yacc.c  */
#line 171 "y.tab.c"

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
     DST = 258,
     SRC = 259,
     HOST = 260,
     GATEWAY = 261,
     NET = 262,
     MASK = 263,
     PORT = 264,
     LESS = 265,
     GREATER = 266,
     PROTO = 267,
     PROTOCHAIN = 268,
     CBYTE = 269,
     ARP = 270,
     RARP = 271,
     IP = 272,
     SCTP = 273,
     TCP = 274,
     UDP = 275,
     ICMP = 276,
     IGMP = 277,
     IGRP = 278,
     PIM = 279,
     VRRP = 280,
     ATALK = 281,
     AARP = 282,
     DECNET = 283,
     LAT = 284,
     SCA = 285,
     MOPRC = 286,
     MOPDL = 287,
     TK_BROADCAST = 288,
     TK_MULTICAST = 289,
     NUM = 290,
     INBOUND = 291,
     OUTBOUND = 292,
     LINK = 293,
     GEQ = 294,
     LEQ = 295,
     NEQ = 296,
     ID = 297,
     EID = 298,
     HID = 299,
     HID6 = 300,
     AID = 301,
     LSH = 302,
     RSH = 303,
     LEN = 304,
     IPV6 = 305,
     ICMPV6 = 306,
     AH = 307,
     ESP = 308,
     VLAN = 309,
     ISO = 310,
     ESIS = 311,
     CLNP = 312,
     ISIS = 313,
     L1 = 314,
     L2 = 315,
     IIH = 316,
     LSP = 317,
     SNP = 318,
     CSNP = 319,
     PSNP = 320,
     STP = 321,
     IPX = 322,
     NETBEUI = 323,
     LANE = 324,
     LLC = 325,
     METAC = 326,
     BCC = 327,
     SC = 328,
     ILMIC = 329,
     OAMF4EC = 330,
     OAMF4SC = 331,
     OAM = 332,
     OAMF4 = 333,
     CONNECTMSG = 334,
     METACONNECT = 335,
     VPI = 336,
     VCI = 337,
     AND = 338,
     OR = 339,
     UMINUS = 340
   };
#endif
/* Tokens.  */
#define DST 258
#define SRC 259
#define HOST 260
#define GATEWAY 261
#define NET 262
#define MASK 263
#define PORT 264
#define LESS 265
#define GREATER 266
#define PROTO 267
#define PROTOCHAIN 268
#define CBYTE 269
#define ARP 270
#define RARP 271
#define IP 272
#define SCTP 273
#define TCP 274
#define UDP 275
#define ICMP 276
#define IGMP 277
#define IGRP 278
#define PIM 279
#define VRRP 280
#define ATALK 281
#define AARP 282
#define DECNET 283
#define LAT 284
#define SCA 285
#define MOPRC 286
#define MOPDL 287
#define TK_BROADCAST 288
#define TK_MULTICAST 289
#define NUM 290
#define INBOUND 291
#define OUTBOUND 292
#define LINK 293
#define GEQ 294
#define LEQ 295
#define NEQ 296
#define ID 297
#define EID 298
#define HID 299
#define HID6 300
#define AID 301
#define LSH 302
#define RSH 303
#define LEN 304
#define IPV6 305
#define ICMPV6 306
#define AH 307
#define ESP 308
#define VLAN 309
#define ISO 310
#define ESIS 311
#define CLNP 312
#define ISIS 313
#define L1 314
#define L2 315
#define IIH 316
#define LSP 317
#define SNP 318
#define CSNP 319
#define PSNP 320
#define STP 321
#define IPX 322
#define NETBEUI 323
#define LANE 324
#define LLC 325
#define METAC 326
#define BCC 327
#define SC 328
#define ILMIC 329
#define OAMF4EC 330
#define OAMF4SC 331
#define OAM 332
#define OAMF4 333
#define CONNECTMSG 334
#define METACONNECT 335
#define VPI 336
#define VCI 337
#define AND 338
#define OR 339
#define UMINUS 340




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 90 "grammar.y"

	int i;
	bpf_u_int32 h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct arth *a;
	struct {
		struct qual q;
		int atmfieldtype;
		struct block *b;
	} blk;
	struct block *rblk;



/* Line 214 of yacc.c  */
#line 394 "y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 406 "y.tab.c"

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   533

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  101
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  33
/* YYNRULES -- Number of rules.  */
#define YYNRULES  154
/* YYNRULES -- Number of states.  */
#define YYNSTATES  210

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   340

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    85,     2,     2,     2,     2,    87,     2,
      94,    93,    90,    88,     2,    89,     2,    91,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   100,     2,
      97,    96,    95,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    98,     2,    99,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    86,     2,     2,     2,     2,     2,
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
      92
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     8,     9,    11,    15,    19,    23,
      27,    29,    31,    33,    35,    39,    41,    45,    49,    51,
      55,    57,    59,    61,    64,    66,    68,    70,    74,    78,
      80,    82,    84,    87,    91,    94,    97,   100,   103,   106,
     109,   113,   115,   119,   123,   125,   127,   129,   132,   134,
     135,   137,   139,   143,   147,   151,   155,   157,   159,   161,
     163,   165,   167,   169,   171,   173,   175,   177,   179,   181,
     183,   185,   187,   189,   191,   193,   195,   197,   199,   201,
     203,   205,   207,   209,   211,   213,   215,   217,   219,   221,
     223,   225,   227,   229,   231,   233,   235,   237,   240,   243,
     246,   249,   254,   256,   258,   261,   263,   265,   267,   269,
     271,   273,   275,   277,   279,   284,   291,   295,   299,   303,
     307,   311,   315,   319,   323,   326,   330,   332,   334,   336,
     338,   340,   342,   344,   348,   350,   352,   354,   356,   358,
     360,   362,   364,   366,   368,   370,   372,   374,   376,   378,
     381,   384,   388,   390,   392
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     102,     0,    -1,   103,   104,    -1,   103,    -1,    -1,   113,
      -1,   104,   105,   113,    -1,   104,   105,   107,    -1,   104,
     106,   113,    -1,   104,   106,   107,    -1,    83,    -1,    84,
      -1,   108,    -1,   127,    -1,   110,   111,    93,    -1,    42,
      -1,    44,    91,    35,    -1,    44,     8,    44,    -1,    44,
      -1,    45,    91,    35,    -1,    45,    -1,    43,    -1,    46,
      -1,   109,   107,    -1,    85,    -1,    94,    -1,   108,    -1,
     112,   105,   107,    -1,   112,   106,   107,    -1,   127,    -1,
     111,    -1,   115,    -1,   109,   113,    -1,   116,   117,   118,
      -1,   116,   117,    -1,   116,   118,    -1,   116,    12,    -1,
     116,    13,    -1,   116,   119,    -1,   114,   107,    -1,   110,
     104,    93,    -1,   120,    -1,   124,   122,   124,    -1,   124,
     123,   124,    -1,   121,    -1,   128,    -1,   129,    -1,   130,
     131,    -1,   120,    -1,    -1,     4,    -1,     3,    -1,     4,
      84,     3,    -1,     3,    84,     4,    -1,     4,    83,     3,
      -1,     3,    83,     4,    -1,     5,    -1,     7,    -1,     9,
      -1,     6,    -1,    38,    -1,    17,    -1,    15,    -1,    16,
      -1,    18,    -1,    19,    -1,    20,    -1,    21,    -1,    22,
      -1,    23,    -1,    24,    -1,    25,    -1,    26,    -1,    27,
      -1,    28,    -1,    29,    -1,    30,    -1,    32,    -1,    31,
      -1,    50,    -1,    51,    -1,    52,    -1,    53,    -1,    55,
      -1,    56,    -1,    58,    -1,    59,    -1,    60,    -1,    61,
      -1,    62,    -1,    63,    -1,    65,    -1,    64,    -1,    57,
      -1,    66,    -1,    67,    -1,    68,    -1,   116,    33,    -1,
     116,    34,    -1,    10,    35,    -1,    11,    35,    -1,    14,
      35,   126,    35,    -1,    36,    -1,    37,    -1,    54,   127,
      -1,    54,    -1,    95,    -1,    39,    -1,    96,    -1,    40,
      -1,    97,    -1,    41,    -1,   127,    -1,   125,    -1,   120,
      98,   124,    99,    -1,   120,    98,   124,   100,    35,    99,
      -1,   124,    88,   124,    -1,   124,    89,   124,    -1,   124,
      90,   124,    -1,   124,    91,   124,    -1,   124,    87,   124,
      -1,   124,    86,   124,    -1,   124,    47,   124,    -1,   124,
      48,   124,    -1,    89,   124,    -1,   110,   125,    93,    -1,
      49,    -1,    87,    -1,    86,    -1,    97,    -1,    95,    -1,
      96,    -1,    35,    -1,   110,   127,    93,    -1,    69,    -1,
      70,    -1,    71,    -1,    72,    -1,    75,    -1,    76,    -1,
      73,    -1,    74,    -1,    77,    -1,    78,    -1,    79,    -1,
      80,    -1,    81,    -1,    82,    -1,   132,    -1,   122,    35,
      -1,   123,    35,    -1,   110,   133,    93,    -1,    35,    -1,
     132,    -1,   133,   106,   132,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   152,   152,   156,   158,   160,   161,   162,   163,   164,
     166,   168,   170,   171,   173,   175,   176,   178,   180,   185,
     194,   203,   212,   221,   223,   225,   227,   228,   229,   231,
     233,   235,   236,   238,   239,   240,   241,   242,   243,   245,
     246,   247,   248,   250,   252,   253,   254,   255,   258,   259,
     262,   263,   264,   265,   266,   267,   270,   271,   272,   275,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   325,   326,   327,   329,
     330,   331,   333,   334,   336,   337,   338,   339,   340,   341,
     342,   343,   344,   345,   346,   347,   348,   350,   351,   352,
     353,   354,   356,   357,   359,   360,   361,   362,   363,   364,
     365,   366,   368,   369,   370,   371,   374,   375,   377,   378,
     379,   380,   382,   389,   390
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DST", "SRC", "HOST", "GATEWAY", "NET",
  "MASK", "PORT", "LESS", "GREATER", "PROTO", "PROTOCHAIN", "CBYTE", "ARP",
  "RARP", "IP", "SCTP", "TCP", "UDP", "ICMP", "IGMP", "IGRP", "PIM",
  "VRRP", "ATALK", "AARP", "DECNET", "LAT", "SCA", "MOPRC", "MOPDL",
  "TK_BROADCAST", "TK_MULTICAST", "NUM", "INBOUND", "OUTBOUND", "LINK",
  "GEQ", "LEQ", "NEQ", "ID", "EID", "HID", "HID6", "AID", "LSH", "RSH",
  "LEN", "IPV6", "ICMPV6", "AH", "ESP", "VLAN", "ISO", "ESIS", "CLNP",
  "ISIS", "L1", "L2", "IIH", "LSP", "SNP", "CSNP", "PSNP", "STP", "IPX",
  "NETBEUI", "LANE", "LLC", "METAC", "BCC", "SC", "ILMIC", "OAMF4EC",
  "OAMF4SC", "OAM", "OAMF4", "CONNECTMSG", "METACONNECT", "VPI", "VCI",
  "AND", "OR", "'!'", "'|'", "'&'", "'+'", "'-'", "'*'", "'/'", "UMINUS",
  "')'", "'('", "'>'", "'='", "'<'", "'['", "']'", "':'", "$accept",
  "prog", "null", "expr", "and", "or", "id", "nid", "not", "paren", "pid",
  "qid", "term", "head", "rterm", "pqual", "dqual", "aqual", "ndaqual",
  "pname", "other", "relop", "irelop", "arth", "narth", "byteop", "pnum",
  "atmtype", "atmmultitype", "atmfield", "atmvalue", "atmfieldvalue",
  "atmlistvalue", 0
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
     335,   336,   337,   338,   339,    33,   124,    38,    43,    45,
      42,    47,   340,    41,    40,    62,    61,    60,    91,    93,
      58
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   101,   102,   102,   103,   104,   104,   104,   104,   104,
     105,   106,   107,   107,   107,   108,   108,   108,   108,   108,
     108,   108,   108,   108,   109,   110,   111,   111,   111,   112,
     112,   113,   113,   114,   114,   114,   114,   114,   114,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   116,   116,
     117,   117,   117,   117,   117,   117,   118,   118,   118,   119,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   121,   121,   121,
     121,   121,   121,   121,   121,   121,   122,   122,   122,   123,
     123,   123,   124,   124,   125,   125,   125,   125,   125,   125,
     125,   125,   125,   125,   125,   125,   125,   126,   126,   126,
     126,   126,   127,   127,   128,   128,   128,   128,   128,   128,
     128,   128,   129,   129,   129,   129,   130,   130,   131,   131,
     131,   131,   132,   133,   133
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     0,     1,     3,     3,     3,     3,
       1,     1,     1,     1,     3,     1,     3,     3,     1,     3,
       1,     1,     1,     2,     1,     1,     1,     3,     3,     1,
       1,     1,     2,     3,     2,     2,     2,     2,     2,     2,
       3,     1,     3,     3,     1,     1,     1,     2,     1,     0,
       1,     1,     3,     3,     3,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       2,     4,     1,     1,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     4,     6,     3,     3,     3,     3,
       3,     3,     3,     3,     2,     3,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     3,     1,     1,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       4,     0,    49,     1,     0,     0,     0,    62,    63,    61,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    78,    77,   132,   102,   103,    60,   126,
      79,    80,    81,    82,   105,    83,    84,    93,    85,    86,
      87,    88,    89,    90,    92,    91,    94,    95,    96,   134,
     135,   136,   137,   140,   141,   138,   139,   142,   143,   144,
     145,   146,   147,    24,     0,    25,     2,    49,    49,     5,
       0,    31,     0,    48,    44,     0,   113,   112,    45,    46,
       0,    99,   100,     0,     0,   104,     0,     0,   124,    10,
      11,    49,    49,    32,     0,   113,   112,    15,    21,    18,
      20,    22,    39,    12,     0,     0,    13,    51,    50,    56,
      59,    57,    58,    36,    37,    97,    98,    34,    35,    38,
       0,   107,   109,   111,     0,     0,     0,     0,     0,     0,
       0,     0,   106,   108,   110,     0,     0,   152,     0,     0,
       0,    47,   148,   128,   127,   130,   131,   129,     0,     0,
       0,     7,    49,    49,     6,   112,     9,     8,    40,   125,
     133,     0,     0,     0,    23,    26,    30,     0,    29,     0,
       0,     0,     0,    33,     0,   122,   123,   121,   120,   116,
     117,   118,   119,    42,    43,   153,     0,   149,   150,   101,
     112,    17,    16,    19,    14,     0,     0,    55,    53,    54,
      52,   114,     0,   151,     0,    27,    28,     0,   154,   115
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    94,    91,    92,   164,   103,   104,    86,
     166,   167,    69,    70,    71,    72,   117,   118,   119,    87,
      74,   135,   136,    75,    76,   148,    77,    78,    79,    80,
     141,   142,   186
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -154
static const yytype_int16 yypact[] =
{
    -154,    13,   160,  -154,   -20,    -6,     8,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,   -18,  -154,  -154,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,  -154,
    -154,  -154,  -154,  -154,   402,  -154,   -65,   322,   322,  -154,
      79,  -154,   495,     3,  -154,   436,  -154,  -154,  -154,  -154,
      21,  -154,  -154,   -75,   -18,  -154,   402,   -51,  -154,  -154,
    -154,   241,   241,  -154,   -57,   -52,   -41,  -154,  -154,    -3,
     -37,  -154,  -154,  -154,    79,    79,  -154,   -38,   -34,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,  -154,    35,  -154,  -154,
     402,  -154,  -154,  -154,   402,   402,   402,   402,   402,   402,
     402,   402,  -154,  -154,  -154,   402,   402,  -154,    32,    42,
      44,  -154,  -154,  -154,  -154,  -154,  -154,  -154,    45,   -41,
     277,  -154,   241,   241,  -154,    16,  -154,  -154,  -154,  -154,
    -154,    37,    48,    49,  -154,  -154,   -36,   -65,   -41,    81,
      88,   103,   104,  -154,    57,    22,    22,   391,   397,   -27,
     -27,  -154,  -154,   277,   277,  -154,   -59,  -154,  -154,  -154,
     -45,  -154,  -154,  -154,  -154,    79,    79,  -154,  -154,  -154,
    -154,  -154,    73,  -154,    32,  -154,  -154,    27,  -154,  -154
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -154,  -154,  -154,   127,   -35,  -153,   -68,   -98,     2,    -2,
    -154,  -154,   -61,  -154,  -154,  -154,  -154,    14,  -154,     6,
    -154,    50,    53,   385,   -58,  -154,   -33,  -154,  -154,  -154,
    -154,  -129,  -154
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -42
static const yytype_int16 yytable[] =
{
      68,    85,   102,   -41,    67,   161,    93,   165,    73,   185,
      95,   143,   144,     3,   196,    81,   -13,    25,    89,    90,
     145,   146,   147,   151,   156,    90,    89,    90,    95,    82,
     154,   157,    84,   204,   203,    96,   158,   106,   -29,   -29,
     109,   159,   111,    83,   112,   169,   170,   120,   160,   171,
     172,   149,   160,    96,   163,   165,   137,   194,   155,   155,
     121,   122,   123,   130,   131,    68,    68,   137,   105,    67,
      67,   106,   168,    73,    73,   208,    65,   187,   138,   188,
     189,   191,    84,   192,   193,   197,   -41,   -41,   162,   153,
     153,    93,   198,   152,   152,    95,   -41,    73,    73,   -13,
     -13,   120,   105,    84,   124,   125,   199,   200,   207,   -13,
     128,   129,   130,   131,    25,    65,   132,   133,   134,   155,
     190,    97,    98,    99,   100,   101,   209,   205,   206,    66,
     139,   173,   195,   140,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   126,   127,   128,   129,   130,   131,     0,
     153,    68,     0,     0,   152,   152,   201,   202,    73,    73,
      -3,     0,   106,   106,    63,     0,     0,     0,     0,     0,
       4,     5,     0,    65,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,   105,   105,    25,    26,    27,    28,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     0,     0,    63,     0,     0,     0,    64,
       0,     4,     5,     0,    65,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,    25,    26,    27,    28,
       0,     0,     0,    97,    98,    99,   100,   101,     0,     0,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,   124,   125,    63,     0,     0,     0,
      64,     0,     4,     5,     0,    65,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,    25,    26,    27,
      28,     0,     0,   126,   127,   128,   129,   130,   131,     0,
       0,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     0,     0,    63,     0,     0,
       0,    64,     0,     0,     0,     0,    65,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,    25,   124,   125,
      28,     0,     0,     0,   124,   125,     0,     0,     0,    88,
       0,    29,    30,    31,    32,    33,     0,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,   150,     0,     0,     0,   121,   122,   123,   127,   128,
     129,   130,   131,   124,   125,   128,   129,   130,   131,     0,
       0,    64,     0,     0,     0,     0,    65,     0,   107,   108,
     109,   110,   111,     0,   112,   174,     0,   113,   114,   175,
     176,   177,   178,   179,   180,   181,   182,     0,     0,     0,
     183,   184,   126,   127,   128,   129,   130,   131,   115,   116,
       0,   132,   133,   134
};

static const yytype_int16 yycheck[] =
{
       2,    34,    70,     0,     2,     8,    67,   105,     2,   138,
      68,    86,    87,     0,   167,    35,     0,    35,    83,    84,
      95,    96,    97,    91,    92,    84,    83,    84,    86,    35,
      91,    92,    34,   186,    93,    68,    93,    70,    83,    84,
       5,    93,     7,    35,     9,    83,    84,    98,    93,    83,
      84,    84,    93,    86,    91,   153,    35,    93,    91,    92,
      39,    40,    41,    90,    91,    67,    68,    35,    70,    67,
      68,   104,   105,    67,    68,   204,    94,    35,    80,    35,
      35,    44,    84,    35,    35,     4,    83,    84,    91,    91,
      92,   152,     4,    91,    92,   153,    93,    91,    92,    83,
      84,    98,   104,   105,    47,    48,     3,     3,    35,    93,
      88,    89,    90,    91,    35,    94,    95,    96,    97,   152,
     153,    42,    43,    44,    45,    46,    99,   195,   196,     2,
      80,   117,   167,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    87,    88,    89,    90,    91,    -1,
     152,   153,    -1,    -1,   152,   153,    99,   100,   152,   153,
       0,    -1,   195,   196,    85,    -1,    -1,    -1,    -1,    -1,
      10,    11,    -1,    94,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,   195,   196,    35,    36,    37,    38,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    -1,    -1,    85,    -1,    -1,    -1,    89,
      -1,    10,    11,    -1,    94,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    -1,    -1,    35,    36,    37,    38,
      -1,    -1,    -1,    42,    43,    44,    45,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    47,    48,    85,    -1,    -1,    -1,
      89,    -1,    10,    11,    -1,    94,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    35,    36,    37,
      38,    -1,    -1,    86,    87,    88,    89,    90,    91,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    -1,    -1,    85,    -1,    -1,
      -1,    89,    -1,    -1,    -1,    -1,    94,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    35,    47,    48,
      38,    -1,    -1,    -1,    47,    48,    -1,    -1,    -1,    64,
      -1,    49,    50,    51,    52,    53,    -1,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    86,    -1,    -1,    -1,    39,    40,    41,    87,    88,
      89,    90,    91,    47,    48,    88,    89,    90,    91,    -1,
      -1,    89,    -1,    -1,    -1,    -1,    94,    -1,     3,     4,
       5,     6,     7,    -1,     9,   120,    -1,    12,    13,   124,
     125,   126,   127,   128,   129,   130,   131,    -1,    -1,    -1,
     135,   136,    86,    87,    88,    89,    90,    91,    33,    34,
      -1,    95,    96,    97
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   102,   103,     0,    10,    11,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    35,    36,    37,    38,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    85,    89,    94,   104,   109,   110,   113,
     114,   115,   116,   120,   121,   124,   125,   127,   128,   129,
     130,    35,    35,    35,   110,   127,   110,   120,   124,    83,
      84,   105,   106,   113,   104,   125,   127,    42,    43,    44,
      45,    46,   107,   108,   109,   110,   127,     3,     4,     5,
       6,     7,     9,    12,    13,    33,    34,   117,   118,   119,
      98,    39,    40,    41,    47,    48,    86,    87,    88,    89,
      90,    91,    95,    96,    97,   122,   123,    35,   110,   122,
     123,   131,   132,    86,    87,    95,    96,    97,   126,   127,
     124,   107,   109,   110,   113,   127,   107,   113,    93,    93,
      93,     8,    91,    91,   107,   108,   111,   112,   127,    83,
      84,    83,    84,   118,   124,   124,   124,   124,   124,   124,
     124,   124,   124,   124,   124,   132,   133,    35,    35,    35,
     127,    44,    35,    35,    93,   105,   106,     4,     4,     3,
       3,    99,   100,    93,   106,   107,   107,    35,   132,    99
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
        case 2:

/* Line 1455 of yacc.c  */
#line 153 "grammar.y"
    {
	finish_parse((yyvsp[(2) - (2)].blk).b);
}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 158 "grammar.y"
    { (yyval.blk).q = qerr; }
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 161 "grammar.y"
    { gen_and((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 162 "grammar.y"
    { gen_and((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 163 "grammar.y"
    { gen_or((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 164 "grammar.y"
    { gen_or((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 166 "grammar.y"
    { (yyval.blk) = (yyvsp[(0) - (1)].blk); }
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 168 "grammar.y"
    { (yyval.blk) = (yyvsp[(0) - (1)].blk); }
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 171 "grammar.y"
    { (yyval.blk).b = gen_ncode(NULL, (bpf_u_int32)(yyvsp[(1) - (1)].i),
						   (yyval.blk).q = (yyvsp[(0) - (1)].blk).q); }
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 173 "grammar.y"
    { (yyval.blk) = (yyvsp[(2) - (3)].blk); }
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 175 "grammar.y"
    { (yyval.blk).b = gen_scode((yyvsp[(1) - (1)].s), (yyval.blk).q = (yyvsp[(0) - (1)].blk).q); }
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 176 "grammar.y"
    { (yyval.blk).b = gen_mcode((yyvsp[(1) - (3)].s), NULL, (yyvsp[(3) - (3)].i),
				    (yyval.blk).q = (yyvsp[(0) - (3)].blk).q); }
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 178 "grammar.y"
    { (yyval.blk).b = gen_mcode((yyvsp[(1) - (3)].s), (yyvsp[(3) - (3)].s), 0,
				    (yyval.blk).q = (yyvsp[(0) - (3)].blk).q); }
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 180 "grammar.y"
    {
				  /* Decide how to parse HID based on proto */
				  (yyval.blk).q = (yyvsp[(0) - (1)].blk).q;
				  (yyval.blk).b = gen_ncode((yyvsp[(1) - (1)].s), 0, (yyval.blk).q);
				}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 185 "grammar.y"
    {
#ifdef INET6
				  (yyval.blk).b = gen_mcode6((yyvsp[(1) - (3)].s), NULL, (yyvsp[(3) - (3)].i),
				    (yyval.blk).q = (yyvsp[(0) - (3)].blk).q);
#else
				  bpf_error("'ip6addr/prefixlen' not supported "
					"in this configuration");
#endif /*INET6*/
				}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 194 "grammar.y"
    {
#ifdef INET6
				  (yyval.blk).b = gen_mcode6((yyvsp[(1) - (1)].s), 0, 128,
				    (yyval.blk).q = (yyvsp[(0) - (1)].blk).q);
#else
				  bpf_error("'ip6addr' not supported "
					"in this configuration");
#endif /*INET6*/
				}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 203 "grammar.y"
    { 
				  (yyval.blk).b = gen_ecode((yyvsp[(1) - (1)].e), (yyval.blk).q = (yyvsp[(0) - (1)].blk).q);
				  /*
				   * $1 was allocated by "pcap_ether_aton()",
				   * so we must free it now that we're done
				   * with it.
				   */
				  free((yyvsp[(1) - (1)].e));
				}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 212 "grammar.y"
    {
				  (yyval.blk).b = gen_acode((yyvsp[(1) - (1)].e), (yyval.blk).q = (yyvsp[(0) - (1)].blk).q);
				  /*
				   * $1 was allocated by "pcap_ether_aton()",
				   * so we must free it now that we're done
				   * with it.
				   */
				  free((yyvsp[(1) - (1)].e));
				}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 221 "grammar.y"
    { gen_not((yyvsp[(2) - (2)].blk).b); (yyval.blk) = (yyvsp[(2) - (2)].blk); }
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 223 "grammar.y"
    { (yyval.blk) = (yyvsp[(0) - (1)].blk); }
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 225 "grammar.y"
    { (yyval.blk) = (yyvsp[(0) - (1)].blk); }
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 228 "grammar.y"
    { gen_and((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 229 "grammar.y"
    { gen_or((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 231 "grammar.y"
    { (yyval.blk).b = gen_ncode(NULL, (bpf_u_int32)(yyvsp[(1) - (1)].i),
						   (yyval.blk).q = (yyvsp[(0) - (1)].blk).q); }
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 236 "grammar.y"
    { gen_not((yyvsp[(2) - (2)].blk).b); (yyval.blk) = (yyvsp[(2) - (2)].blk); }
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 238 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (3)].i), (yyvsp[(2) - (3)].i), (yyvsp[(3) - (3)].i)); }
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 239 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (2)].i), (yyvsp[(2) - (2)].i), Q_DEFAULT); }
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 240 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (2)].i), Q_DEFAULT, (yyvsp[(2) - (2)].i)); }
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 241 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (2)].i), Q_DEFAULT, Q_PROTO); }
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 242 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (2)].i), Q_DEFAULT, Q_PROTOCHAIN); }
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 243 "grammar.y"
    { QSET((yyval.blk).q, (yyvsp[(1) - (2)].i), Q_DEFAULT, (yyvsp[(2) - (2)].i)); }
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 245 "grammar.y"
    { (yyval.blk) = (yyvsp[(2) - (2)].blk); }
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 246 "grammar.y"
    { (yyval.blk).b = (yyvsp[(2) - (3)].blk).b; (yyval.blk).q = (yyvsp[(1) - (3)].blk).q; }
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 247 "grammar.y"
    { (yyval.blk).b = gen_proto_abbrev((yyvsp[(1) - (1)].i)); (yyval.blk).q = qerr; }
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 248 "grammar.y"
    { (yyval.blk).b = gen_relation((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a), 0);
				  (yyval.blk).q = qerr; }
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 250 "grammar.y"
    { (yyval.blk).b = gen_relation((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a), 1);
				  (yyval.blk).q = qerr; }
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 252 "grammar.y"
    { (yyval.blk).b = (yyvsp[(1) - (1)].rblk); (yyval.blk).q = qerr; }
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 253 "grammar.y"
    { (yyval.blk).b = gen_atmtype_abbrev((yyvsp[(1) - (1)].i)); (yyval.blk).q = qerr; }
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 254 "grammar.y"
    { (yyval.blk).b = gen_atmmulti_abbrev((yyvsp[(1) - (1)].i)); (yyval.blk).q = qerr; }
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 255 "grammar.y"
    { (yyval.blk).b = (yyvsp[(2) - (2)].blk).b; (yyval.blk).q = qerr; }
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 259 "grammar.y"
    { (yyval.i) = Q_DEFAULT; }
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 262 "grammar.y"
    { (yyval.i) = Q_SRC; }
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 263 "grammar.y"
    { (yyval.i) = Q_DST; }
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 264 "grammar.y"
    { (yyval.i) = Q_OR; }
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 265 "grammar.y"
    { (yyval.i) = Q_OR; }
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 266 "grammar.y"
    { (yyval.i) = Q_AND; }
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 267 "grammar.y"
    { (yyval.i) = Q_AND; }
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 270 "grammar.y"
    { (yyval.i) = Q_HOST; }
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 271 "grammar.y"
    { (yyval.i) = Q_NET; }
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 272 "grammar.y"
    { (yyval.i) = Q_PORT; }
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 275 "grammar.y"
    { (yyval.i) = Q_GATEWAY; }
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 277 "grammar.y"
    { (yyval.i) = Q_LINK; }
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 278 "grammar.y"
    { (yyval.i) = Q_IP; }
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 279 "grammar.y"
    { (yyval.i) = Q_ARP; }
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 280 "grammar.y"
    { (yyval.i) = Q_RARP; }
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 281 "grammar.y"
    { (yyval.i) = Q_SCTP; }
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 282 "grammar.y"
    { (yyval.i) = Q_TCP; }
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 283 "grammar.y"
    { (yyval.i) = Q_UDP; }
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 284 "grammar.y"
    { (yyval.i) = Q_ICMP; }
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 285 "grammar.y"
    { (yyval.i) = Q_IGMP; }
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 286 "grammar.y"
    { (yyval.i) = Q_IGRP; }
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 287 "grammar.y"
    { (yyval.i) = Q_PIM; }
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 288 "grammar.y"
    { (yyval.i) = Q_VRRP; }
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 289 "grammar.y"
    { (yyval.i) = Q_ATALK; }
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 290 "grammar.y"
    { (yyval.i) = Q_AARP; }
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 291 "grammar.y"
    { (yyval.i) = Q_DECNET; }
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 292 "grammar.y"
    { (yyval.i) = Q_LAT; }
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 293 "grammar.y"
    { (yyval.i) = Q_SCA; }
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 294 "grammar.y"
    { (yyval.i) = Q_MOPDL; }
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 295 "grammar.y"
    { (yyval.i) = Q_MOPRC; }
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 296 "grammar.y"
    { (yyval.i) = Q_IPV6; }
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 297 "grammar.y"
    { (yyval.i) = Q_ICMPV6; }
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 298 "grammar.y"
    { (yyval.i) = Q_AH; }
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 299 "grammar.y"
    { (yyval.i) = Q_ESP; }
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 300 "grammar.y"
    { (yyval.i) = Q_ISO; }
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 301 "grammar.y"
    { (yyval.i) = Q_ESIS; }
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 302 "grammar.y"
    { (yyval.i) = Q_ISIS; }
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 303 "grammar.y"
    { (yyval.i) = Q_ISIS_L1; }
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 304 "grammar.y"
    { (yyval.i) = Q_ISIS_L2; }
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 305 "grammar.y"
    { (yyval.i) = Q_ISIS_IIH; }
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 306 "grammar.y"
    { (yyval.i) = Q_ISIS_LSP; }
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 307 "grammar.y"
    { (yyval.i) = Q_ISIS_SNP; }
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 308 "grammar.y"
    { (yyval.i) = Q_ISIS_PSNP; }
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 309 "grammar.y"
    { (yyval.i) = Q_ISIS_CSNP; }
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 310 "grammar.y"
    { (yyval.i) = Q_CLNP; }
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 311 "grammar.y"
    { (yyval.i) = Q_STP; }
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 312 "grammar.y"
    { (yyval.i) = Q_IPX; }
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 313 "grammar.y"
    { (yyval.i) = Q_NETBEUI; }
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 315 "grammar.y"
    { (yyval.rblk) = gen_broadcast((yyvsp[(1) - (2)].i)); }
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 316 "grammar.y"
    { (yyval.rblk) = gen_multicast((yyvsp[(1) - (2)].i)); }
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 317 "grammar.y"
    { (yyval.rblk) = gen_less((yyvsp[(2) - (2)].i)); }
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 318 "grammar.y"
    { (yyval.rblk) = gen_greater((yyvsp[(2) - (2)].i)); }
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 319 "grammar.y"
    { (yyval.rblk) = gen_byteop((yyvsp[(3) - (4)].i), (yyvsp[(2) - (4)].i), (yyvsp[(4) - (4)].i)); }
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 320 "grammar.y"
    { (yyval.rblk) = gen_inbound(0); }
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 321 "grammar.y"
    { (yyval.rblk) = gen_inbound(1); }
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 322 "grammar.y"
    { (yyval.rblk) = gen_vlan((yyvsp[(2) - (2)].i)); }
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 323 "grammar.y"
    { (yyval.rblk) = gen_vlan(-1); }
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 325 "grammar.y"
    { (yyval.i) = BPF_JGT; }
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 326 "grammar.y"
    { (yyval.i) = BPF_JGE; }
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 327 "grammar.y"
    { (yyval.i) = BPF_JEQ; }
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 329 "grammar.y"
    { (yyval.i) = BPF_JGT; }
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 330 "grammar.y"
    { (yyval.i) = BPF_JGE; }
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 331 "grammar.y"
    { (yyval.i) = BPF_JEQ; }
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 333 "grammar.y"
    { (yyval.a) = gen_loadi((yyvsp[(1) - (1)].i)); }
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 336 "grammar.y"
    { (yyval.a) = gen_load((yyvsp[(1) - (4)].i), (yyvsp[(3) - (4)].a), 1); }
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 337 "grammar.y"
    { (yyval.a) = gen_load((yyvsp[(1) - (6)].i), (yyvsp[(3) - (6)].a), (yyvsp[(5) - (6)].i)); }
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 338 "grammar.y"
    { (yyval.a) = gen_arth(BPF_ADD, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 339 "grammar.y"
    { (yyval.a) = gen_arth(BPF_SUB, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 340 "grammar.y"
    { (yyval.a) = gen_arth(BPF_MUL, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 341 "grammar.y"
    { (yyval.a) = gen_arth(BPF_DIV, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 342 "grammar.y"
    { (yyval.a) = gen_arth(BPF_AND, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 343 "grammar.y"
    { (yyval.a) = gen_arth(BPF_OR, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 344 "grammar.y"
    { (yyval.a) = gen_arth(BPF_LSH, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 345 "grammar.y"
    { (yyval.a) = gen_arth(BPF_RSH, (yyvsp[(1) - (3)].a), (yyvsp[(3) - (3)].a)); }
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 346 "grammar.y"
    { (yyval.a) = gen_neg((yyvsp[(2) - (2)].a)); }
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 347 "grammar.y"
    { (yyval.a) = (yyvsp[(2) - (3)].a); }
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 348 "grammar.y"
    { (yyval.a) = gen_loadlen(); }
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 350 "grammar.y"
    { (yyval.i) = '&'; }
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 351 "grammar.y"
    { (yyval.i) = '|'; }
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 352 "grammar.y"
    { (yyval.i) = '<'; }
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 353 "grammar.y"
    { (yyval.i) = '>'; }
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 354 "grammar.y"
    { (yyval.i) = '='; }
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 357 "grammar.y"
    { (yyval.i) = (yyvsp[(2) - (3)].i); }
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 359 "grammar.y"
    { (yyval.i) = A_LANE; }
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 360 "grammar.y"
    { (yyval.i) = A_LLC; }
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 361 "grammar.y"
    { (yyval.i) = A_METAC;	}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 362 "grammar.y"
    { (yyval.i) = A_BCC; }
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 363 "grammar.y"
    { (yyval.i) = A_OAMF4EC; }
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 364 "grammar.y"
    { (yyval.i) = A_OAMF4SC; }
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 365 "grammar.y"
    { (yyval.i) = A_SC; }
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 366 "grammar.y"
    { (yyval.i) = A_ILMIC; }
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 368 "grammar.y"
    { (yyval.i) = A_OAM; }
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 369 "grammar.y"
    { (yyval.i) = A_OAMF4; }
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 370 "grammar.y"
    { (yyval.i) = A_CONNECTMSG; }
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 371 "grammar.y"
    { (yyval.i) = A_METACONNECT; }
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 374 "grammar.y"
    { (yyval.blk).atmfieldtype = A_VPI; }
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 375 "grammar.y"
    { (yyval.blk).atmfieldtype = A_VCI; }
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 378 "grammar.y"
    { (yyval.blk).b = gen_atmfield_code((yyvsp[(0) - (2)].blk).atmfieldtype, (u_int)(yyvsp[(2) - (2)].i), (u_int)(yyvsp[(1) - (2)].i), 0); }
    break;

  case 150:

/* Line 1455 of yacc.c  */
#line 379 "grammar.y"
    { (yyval.blk).b = gen_atmfield_code((yyvsp[(0) - (2)].blk).atmfieldtype, (u_int)(yyvsp[(2) - (2)].i), (u_int)(yyvsp[(1) - (2)].i), 1); }
    break;

  case 151:

/* Line 1455 of yacc.c  */
#line 380 "grammar.y"
    { (yyval.blk).b = (yyvsp[(2) - (3)].blk).b; (yyval.blk).q = qerr; }
    break;

  case 152:

/* Line 1455 of yacc.c  */
#line 382 "grammar.y"
    {
	(yyval.blk).atmfieldtype = (yyvsp[(0) - (1)].blk).atmfieldtype;
	if ((yyval.blk).atmfieldtype == A_VPI ||
	    (yyval.blk).atmfieldtype == A_VCI)
		(yyval.blk).b = gen_atmfield_code((yyval.blk).atmfieldtype, (u_int) (yyvsp[(1) - (1)].i), BPF_JEQ, 0);
	}
    break;

  case 154:

/* Line 1455 of yacc.c  */
#line 390 "grammar.y"
    { gen_or((yyvsp[(1) - (3)].blk).b, (yyvsp[(3) - (3)].blk).b); (yyval.blk) = (yyvsp[(3) - (3)].blk); }
    break;



/* Line 1455 of yacc.c  */
#line 2931 "y.tab.c"
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
#line 392 "grammar.y"


