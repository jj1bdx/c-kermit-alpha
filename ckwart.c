#define CKWART_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ckcdeb.h"
#include "ckcsym.h"
#include "ckcasc.h"
#include "ckcker.h"
#include "ckclib.h"

char *wartv = "Wart Version 2.17, 04 February 2024 ";

#ifdef MDEBUG
/* Use the real ones in this module only */
#ifdef malloc
#undef malloc
#endif /* malloc */
#ifdef calloc
#undef calloc
#endif /* calloc */
#ifdef realloc
#undef realloc
#endif /* realloc */
#ifdef free
#undef free
#endif /* free */
#endif /* MDEBUG */

#ifdef MAC
#define VOID void
#endif /* MAC */

/* W A R T */

/*
  A small subset of "lex".

  Authors: Jeff Damens, Frank da Cruz
  Columbia University Center for Computing Activites.
  First released November 1984.
  Copyright (C) 1984, 2024,
    Trustees of Columbia University in the City of New York.
    All rights reserved.  See the C-Kermit COPYING.TXT file or the
    copyright text in the ckcmai.c module for disclaimer and permissions.
*/

/*
 * input format is:
 *  lines to be copied | %state <state names...>
 *  %%
 * <state> | <state,state,...> CHAR  { actions }
 * ...
 *  %%
 *  more lines to be copied
 */

#include "ckcdeb.h"			/* Includes */

#ifdef STRATUS
/* Actually call printf, not our printf-catcher for Kermit */
#ifdef printf
#undef printf
#endif /* printf */
#ifdef fprintf
#undef fprintf
#endif /* fprintf */
#endif /* STRATUS */

#ifdef MAC
/* Same deal for Macintosh */
#ifdef printf
#undef printf
#endif /* printf */
#ifdef fprintf
#undef fprintf
#endif /* fprintf */
#endif /* MAC */

#ifdef UNIX
/* And UNIX */
#ifdef printf
#undef printf
#endif /* printf */
#ifdef fprintf
#undef fprintf
#endif /* fprintf */
#endif /* UNIX */
/*
  The following "char" should be changed to "short", "int", or "long" if your
  wart program will generate more than 127 states.  Since wart is used mainly
  with C-Kermit, which has about 80 states, "char" is adequate.  This keeps
  the program about 3K-4K smaller, which can be critical on 16-bit
  architectures.
*/
#ifdef IRIX60
/*
  Also use short or int if your compiler complains inordinately about
  "integer conversion resulted in a change of sign"...
*/
#define TBL_TYPE "short"		/* C data type of state table */
#else
#define TBL_TYPE "char"			/* C data type of state table */
#endif /* IRIX60 */

#define C_L 014				/* Formfeed */

#define SEP 1				/* Token types */
#define LBRACK 2
#define RBRACK 3
#define WORD 4
#define COMMA 5

/* Storage sizes */

#define MAXSTATES 50			/* max number of states */
#define MAXWORD 50			/* max # of chars/word */
#define SBYTES ((MAXSTATES+6)/8)	/* # of bytes for state bitmask */

/* Name of wart function in generated program */

#ifndef FNAME
#define FNAME "wart"
#endif /* FNAME */

/* Structure for state information */

struct transx {
    CHAR states[SBYTES];		/* included states */
    int anyst;				/* true if this good from any state */
    CHAR inchr;				/* input character */
    int actno;				/* associated action */
    struct transx *nxt;
};					/* next transition */
typedef struct transx *trans;

/* Function prototypes */

#ifdef OS2
typedef VOID WMAINTYPE;
#else
typedef int WMAINTYPE;
#endif  /* OS2 */
_PROTOTYP( WMAINTYPE main, (int argc, char **argv) );
_PROTOTYP( VOID fatal, (char *msg) );
_PROTOTYP( VOID setwstate, (int state, trans t) );
_PROTOTYP( int teststate, (int, trans) );
_PROTOTYP( trans rdinput, (FILE *, FILE *) );
_PROTOTYP( VOID initial, (FILE *, FILE *) );
_PROTOTYP( int isin, (char *, int) );
_PROTOTYP( int isword, (int) );
_PROTOTYP( VOID rdword, (FILE *, char *) );
_PROTOTYP( VOID rdstates, (FILE *, FILE *) );
_PROTOTYP( trans newtrans, (void) );
_PROTOTYP( trans rdrules, (FILE *, FILE *) );
_PROTOTYP( VOID statelist, (FILE *, trans) );
_PROTOTYP( VOID copyact, (FILE *, FILE *, int) );
_PROTOTYP( int faction, (trans, int, int) );
_PROTOTYP( VOID emptytbl, (void) );
_PROTOTYP( VOID addaction, (int, int, int) );
_PROTOTYP( VOID writetbl, (FILE *) );
_PROTOTYP( VOID warray, (FILE *, char *, int [], int, char *) );
_PROTOTYP( VOID prolog, (FILE *) );
_PROTOTYP( VOID epilogue, (FILE *) );
_PROTOTYP( VOID copyrest, (FILE *, FILE *) );
_PROTOTYP( int gettoken, (FILE *) );
_PROTOTYP( VOID rdcmnt, (FILE *) );
_PROTOTYP( VOID clrhash, (void) );
_PROTOTYP( int hash, (char *) );
_PROTOTYP( VOID enter, (char *, int) );
_PROTOTYP( int lkup, (char *) );
_PROTOTYP( static char* copy, (char *s) );

/* Variables and tables */

/* lt 1992-10-08 Begin
 * provide definition for deblog variable
 * ckcdeb.h declares as extern. DECC AXP is strict about ref/def model
 * Variable is unused herein, to the best of my knowledge.
 */
#ifdef VMS
int deblog;
#endif /* VMS */
/* lt 1992-10-08 End
 */

static int lines, nstates, nacts;

static char tokval[MAXWORD];

static int tbl[MAXSTATES*96];

char *tbl_type = TBL_TYPE;

char *txt1 = "\n#define BEGIN state =\n\nint state = 0;\n\nint\n";

char *fname = FNAME;			/* Generated function name goes here */

/* rest of program... */

char *txt2 = "()\n\
{\n\
    int c,actno;\n\
    extern ";

/* Data type of state table is inserted here (short or int) */

char *txt2a =
" tbl[];\n\
    while (1) {\n\
	c = input() - 32;\n\
	debug(F000,\"PROTO input\",ckitoa(state),c+32);\n\
	if (c < 0 || c > 95) c = 0;\n";

char *txt2b = "	if ((actno = tbl[c + state*96]) != -1)\n\
	    switch(actno) {\n";

/* this program's output goes here, followed by final text... */

char *txt3 = "\n	    }\n    }\n}\n\n";


/*
 * turn on the bit associated with the given state
 */
VOID
#ifdef CK_ANSIC
setwstate(int state, trans t)
#else
setwstate(state,t) int state; trans t;
#endif /* CK_ANSIC */
/* setwstate */ {
    int idx,msk;
    idx = state/8;			/* byte associated with state */
    msk = 0x80 >> (state % 8);		/* bit mask for state */
    t->states[idx] |= msk;
}

/*
 * see if the state is involved in the transition
 */
int
#ifdef CK_ANSIC
teststate(int state, trans t)
#else
teststate(state,t) int state; trans t;
#endif /* CK_ANSIC */
/* teststate */ {
    int idx,msk;
    idx = state/8;
    msk = 0x80 >> (state % 8);
    return(t->states[idx] & msk);
}


/*
 * read input from here...
 */
trans
#ifdef CK_ANSIC
rdinput(FILE *infp, FILE *outfp)
#else
rdinput(infp,outfp) FILE *infp,*outfp;
#endif  /* CK_ANSIC */
/* rdinput */ {
    trans x;
    lines = 1;				/* line counter */
    nstates = 0;			/* no states */
    nacts = 0;				/* no actions yet */
    fprintf(outfp,"\n%c* WARNING -- This C source program generated by ",'/');
    fprintf(outfp,"Wart preprocessor. */\n");
    fprintf(outfp,"%c* Do not edit this file; edit the Wart-format ",'/');
    fprintf(outfp,"source file instead, */\n");
    fprintf(outfp,"%c* and then run it through Wart to produce a new ",'/');
    fprintf(outfp,"C source file.     */\n\n");
    fprintf(outfp,"%c* Wart Version Info: */\n",'/');
    fprintf(outfp,"char *wartv = \"%s\";\n\n",wartv);

    initial(infp,outfp);		/* read state names, initial defs */
    prolog(outfp);			/* write out our initial code */
    x = rdrules(infp,outfp);		/* read rules */
    epilogue(outfp);			/* write out epilogue code */
    return(x);
}

/*
 * initial - read initial definitions and state names.  Returns
 * on EOF or %%.
 */
VOID
#ifdef CK_ANSIC
initial(FILE *infp, FILE *outfp)
#else 
initial(infp,outfp) FILE *infp, *outfp;
#endif  /* CK_ANSIC */
/* initial */ {
    int c;
    char wordbuf[MAXWORD];
    while ((c = getc(infp)) != EOF) {
	if (c == '%') {
	    rdword(infp,wordbuf);
	    if (strcmp(wordbuf,"states") == 0)
	      rdstates(infp,outfp);
	    else if (strcmp(wordbuf,"%") == 0) return;
	    else fprintf(outfp,"%%%s",wordbuf);
	}
	else putc(c,outfp);
	if (c == '\n') lines++;
    }
}

/*
 * boolean function to tell if the given character can be part of a word.
 */
int
#ifdef CK_ANSIC
isin(char *s, int c)
#else
isin(s,c) char *s; int c;
#endif /* CK_ANSIC */
/* isin */ {
    for (; *s != '\0'; s++)
      if (*s == (char) c) return(1);
    return(0);
}
int
#ifdef CK_ANSIC
isword(int c)
#else
isword(c) int c;
#endif /* CK_ANSIC */
/* isword */ {
    static char special[] = ".%_-$@";	/* these are allowable */
    return(isalnum(c) || isin(special,c));
}

/*
 * read the next word into the given buffer.
 */
VOID
#ifdef CK_ANSIC
rdword(FILE *fp, char *buf)
#else
rdword(fp,buf) FILE *fp; char *buf;
#endif /* CK_ANSIC */
/* rdword */ {
    int len = 0,c;
    while (isword(c = getc(fp)) && ++len < MAXWORD) *buf++ = (char) c;
    *buf++ = '\0';			/* tie off word */
    ungetc(c,fp);			/* put break char back */
}

/*
 * read state names, up to a newline.
 */
VOID
#ifdef CK_ANSIC
rdstates(FILE *fp, FILE *ofp)
#else
rdstates(fp,ofp) FILE *fp,*ofp;
#endif /* CK_ANSIC */
/* rdstates */ {
    int c;
    char wordbuf[MAXWORD];
    while ((c = getc(fp)) != EOF && c != '\n') {
	if (isspace(c) || c == C_L) continue;	/* skip whitespace */
	ungetc(c,fp);			/* put char back */
	rdword(fp,wordbuf);		/* read the whole word */
	enter(wordbuf,++nstates);	/* put into symbol tbl */
	fprintf(ofp,"#define %s %d\n",wordbuf,nstates);
    }
    lines++;
}

/*
 * Allocate a new, empty transition node
 */
trans
#ifdef CK_ANSIC
newtrans(void)
#else
newtrans()
#endif /* CK_ANSIC */
/* newtrans */  {
    trans new;
    int i;
    new = (trans) malloc(sizeof (struct transx));
    for (i=0; i<SBYTES; i++) new->states[i] = 0;
    new->anyst = 0;
    new->nxt = NULL;
    return(new);
}

/*
 * read all the rules.
 */
trans
#ifdef CK_ANSIC
rdrules(FILE *fp, FILE *out)
#else
rdrules(fp,out) FILE *fp,*out;
#endif /* CK_ANSIC */
/* rdrules */ {
    trans head,cur,prev;
    int curtok;
    head = cur = prev = NULL;
    while ((curtok = gettoken(fp)) != SEP)

      switch(curtok) {
	case LBRACK:
	  if (cur == NULL)
	    cur = newtrans();
	  else
	    fatal("duplicate state list");
	  statelist(fp,cur);		/* set states */
	  continue;			/* prepare to read char */

	case WORD:
	  if ((int)strlen(tokval) != 1)
	    fatal("multiple chars in state");
	  if (cur == NULL) {
	      cur = newtrans();
	      cur->anyst = 1;
	  }
	  cur->actno = ++nacts;
	  cur->inchr = (char) (tokval[0] - 32);
	  if (head == NULL)
	    head = cur;
	  else
	    prev->nxt = cur;
	  prev = cur;
	  cur = NULL;
	  copyact(fp,out,nacts);
	  break;
	default: fatal("bad input format");
      }
    return(head);
}

/*
 * read a list of (comma-separated) states, set them in the
 * given transition.
 */
VOID
#ifdef CK_ANSIC
statelist(FILE *fp, trans t)
#else
statelist(fp,t) FILE *fp; trans t;
#endif /* CK_ANSIC */
/* statelist */ {
    int curtok,sval;
    curtok = COMMA;
    while (curtok != RBRACK) {
	if (curtok != COMMA) fatal("missing comma");
	if ((curtok = gettoken(fp)) != WORD) fatal("missing state name");
	if ((sval = lkup(tokval)) == -1) {
	    fprintf(stderr,"state %s undefined\n",tokval);
	    fatal("undefined state");
	}
	setwstate(sval,t);
	curtok = gettoken(fp);
    }
}

/*
 * copy an action from the input to the output file
 *
 */
VOID
#ifdef CK_ANSIC
copyact(FILE *inp, FILE *outp, int actno)
#else
copyact(inp,outp,actno) FILE *inp,*outp; int actno;
#endif /* CK_ANSIC */
/* copyact */ {
    int c,bcnt;
    fprintf(outp,"case %d:\n",actno);
    while (c = getc(inp), (isspace(c) || c == C_L))
      if (c == '\n') lines++;
    if (c == '{') {
	bcnt = 1;
	fputs("    {",outp);
	while (bcnt > 0 && (c = getc(inp)) != EOF) {
	    if (c == '{') bcnt++;
	    else if (c == '}') bcnt--;
	    else if (c == '\n') lines++;
	    putc(c,outp);
	}
	if (bcnt > 0) fatal("action doesn't end");
    } else {
	while (c != '\n' && c != EOF) {
	    putc(c,outp);
	    c = getc(inp);
	}
	lines++;
    }
    fprintf(outp,"\n    break;\n");
}

/*
 * find the action associated with a given character and state.
 * returns -1 if one can't be found.
 *
 */
int
#ifdef CK_ANSIC
faction(trans hd, int state, int chr)
#else
faction(hd,state,chr) trans hd; int state,chr;
#endif /* CK_ANSIC */
/* faction */ {
    while (hd != NULL) {
	if (hd->anyst || teststate(state,hd))
	  if (hd->inchr == ('.' - 32) || hd->inchr == (char) chr)
	    return(hd->actno);
	hd = hd->nxt;
    }
    return(-1);
}

/*
 * empty the table...
 *
 */
VOID
#ifdef CK_ANSIC
emptytbl(void)
#else
emptytbl()
#endif  /* CK_ANSIC */
{
    int i;
    for (i=0; i<nstates*96; i++) tbl[i] = -1;
}

/*
 * add the specified action to the output for the given state and chr.
 *
 */
VOID
#ifdef CK_ANSIC
addaction(int act, int state, int chr)
#else
addaction(act,state,chr) int act,state,chr;
#endif /* CK_ANSIC */
/* addaction */ {
    tbl[state*96 + chr] = act;
}

VOID
#ifdef CK_ANSIC
writetbl(FILE *fp)
#else
writetbl(fp) FILE *fp;
#endif /* CK_ANSIC */
/* writetbl */ {
    warray(fp,"tbl",tbl,96*(nstates+1),TBL_TYPE);
}

/*
 * write an array to the output file, given its name and size.
 */
VOID
#ifdef CK_ANSIC
warray(FILE *fp, char *nam, int cont[], int siz, char *typ)
#else
warray(fp,nam,cont,siz,typ) FILE *fp; char *nam; int cont[],siz; char *typ;
#endif /* CK_ANSIC */
/* warray */ {
    int i;
    fprintf(fp,"%s %s[] = {\n",typ,nam);
    for (i = 0; i < siz - 1; ) {
	fprintf(fp," %2d,",cont[i]);
	if ((++i % 16) == 0) putc('\n',fp);
    }
    fprintf(fp,"%2d\n};\n",cont[siz-1]);
}
/*
  There was an #ifdef rat's nest here regarding main's return type.
  The following should be equivalent and is much simpler.  OS2 actually
  means IBM OS/2 or MS Windows even though OS/2 itself is long gone.
  -fdc, Fri Sep 18 19:42:48 2020
*/
WMAINTYPE
#ifdef CK_ANSIC
main(int argc, char **argv)
#else
main(argc,argv) int argc; char **argv;
#endif  /* CK_ANSIC */
{
    trans head;
    int state,c;
    FILE *infile,*outfile;

    if (argc > 1) {
	if ((infile = fopen(argv[1],"r")) == NULL) {
	    fprintf(stderr,"Can't open %s\n",argv[1]);
	    fatal("unreadable input file");
	}
    } else infile = stdin;

    if (argc > 2) {
	if ((outfile = fopen(argv[2],"w")) == NULL) {
	    fprintf(stderr,"Can't write to %s\n",argv[2]);
	    fatal("bad output file");
	}
    } else outfile = stdout;

    clrhash();				/* empty hash table */
    head = rdinput(infile,outfile);	/* read input file */
    emptytbl();				/* empty our tables */
    for (state = 0; state <= nstates; state++)
      for (c = 1; c < 96; c++)		/* find actions, */
	addaction(faction(head,state,c),state,c); /* add to tbl */
    writetbl(outfile);
    copyrest(infile,outfile);
    printf("%d states, %d actions\n",nstates,nacts);
    exit(GOOD_EXIT);
}

/*
 * fatal error handler
 */
VOID
#ifdef CK_ANSIC
fatal(char *msg)
#else
fatal(msg) char *msg;
#endif  /* CK_ANSIC */
{
    fprintf(stderr,"error in line %d: %s\n",lines,msg);
    exit(BAD_EXIT);
}

VOID
#ifdef CK_ANSIC
prolog(FILE *outfp)
#else
prolog(outfp) FILE *outfp;
#endif  /* CK_ANSIC */
{   int c;
    while ((c = *txt1++)     != '\0') putc(c,outfp);
    while ((c = *fname++)    != '\0') putc(c,outfp);
    while ((c = *txt2++)     != '\0') putc(c,outfp);
    while ((c = *tbl_type++) != '\0') putc(c,outfp);
    while ((c = *txt2a++)    != '\0') putc(c,outfp);
    while ((c = *txt2b++)    != '\0') putc(c,outfp);
}

VOID
#ifdef CK_ANSIC
epilogue(FILE *outfp)
#else
epilogue(outfp) FILE *outfp;
#endif  /* CK_ANSIC */
{
    int c;
    while ((c = *txt3++) != '\0') putc(c,outfp);
}

VOID
#ifdef CK_ANSIC
copyrest(FILE *in, FILE *out)
#else
copyrest(in,out) FILE *in,*out;
#endif  /* CK_ANSIC */
{
    int c;
    while ((c = getc(in)) != EOF) putc(c,out);
}

/*
 * gettoken - returns token type of next token, sets tokval
 * to the string value of the token if appropriate.
 */
int
#ifdef CK_ANSIC
gettoken(FILE *fp)
#else
gettoken(fp) FILE *fp;
#endif  /* CK_ANSIC */
{
    int c;
    while (1) {				/* loop if reading comments... */
	do {
	    c = getc(fp);
	    if (c == '\n') lines++;
	} while ((isspace(c) || c == C_L)); /* skip whitespace */
	switch(c) {
	  case EOF:
	    return(SEP);
	  case '%':
	    if ((c = getc(fp)) == '%') return(SEP);
	    tokval[0] = '%';
	    tokval[1] = (char) c;
	    rdword(fp,tokval+2);
	    return(WORD);
	  case '<':
	    return(LBRACK);
	  case '>':
	    return(RBRACK);
	  case ',':
	    return(COMMA);
	  case '/':
	    if ((c = getc(fp)) == '*') {
		rdcmnt(fp);		/* skip over the comment */
		continue;
	    } else {			/* and keep looping */
		ungetc(c,fp);		/* put this back into input */
		c = '/';		/* put character back, fall thru */
	    }

	  default:
	    if (isword(c)) {
		ungetc(c,fp);
		rdword(fp,tokval);
		return(WORD);
	    } else fatal("Invalid character in input");
	}
    }
}

/*
 * skip over a comment
 */

VOID
#ifdef CK_ANSIC
rdcmnt(FILE *fp)
#else
rdcmnt(fp) FILE *fp;
#endif  /* CK_ANSIC */
 {
    int c,star,prcnt;
    prcnt = star = 0;			/* no star seen yet */
    while (!((c = getc(fp)) == '/' && star)) {
	if (c == EOF || (prcnt && c == '%')) fatal("Unterminated comment");
	prcnt = (c == '%');
	star = (c == '*');
	if (c == '\n') lines++;
    }
}

/*
 * symbol table management for wart
 *
 * entry points:
 *   clrhash - empty hash table.
 *   enter - enter a name into the symbol table
 *   lkup - find a name's value in the symbol table.
 *
 */

#define HASHSIZE 101			/* # of entries in hash table */

struct sym {
    char *name;				/* symbol name */
    int val;				/* value */
    struct sym *hnxt;			/* next on collision chain */
} *htab[HASHSIZE];			/* the hash table */

/*
 * empty the hash table before using it...
 *
 */
VOID
clrhash() {
    int i;
    for (i=0; i<HASHSIZE; i++) htab[i] = NULL;
}

/*
 * compute the value of the hash for a symbol
 *
 */
int
#ifdef CK_ANSIC
hash(char *name)
#else
hash(name) char *name;
#endif  /* CK_ANSIC */
{
    int sum;
    for (sum = 0; *name != '\0'; name++) sum += (sum + *name);
    sum %= HASHSIZE;			/* take sum mod hashsize */
    if (sum < 0) sum += HASHSIZE;	/* disallow negative hash value */
    return(sum);
}

/*
 * make a private copy of a string...
 *
 */
static char*
#ifdef CK_ANSIC
copy(char *s)
#else
copy(s) char *s;
#endif  /* CK_ANSIC */
{
    char *new;
    new = (char *) malloc((int)strlen(s) + 1);
    strcpy(new,s);
    return(new);
}

/*
 * enter state name into the hash table
 *
 */
VOID
#ifdef CK_ANSIC
enter(char *name, int svalue)
#else
enter(name,svalue) char *name; int svalue;
#endif  /* CK_ANSIC */
{
    int h;
    struct sym *cur;
    if (lkup(name) != -1) {
	fprintf(stderr,"state \"%s\" appears twice...\n", name);
	exit(BAD_EXIT);
    }
    h = hash(name);
    cur = (struct sym *)malloc(sizeof (struct sym));
    cur->name = copy(name);
    cur->val = svalue;
    cur->hnxt = htab[h];
    htab[h] = cur;
}

/*
 * find name in the symbol table, return its value.  Returns -1
 * if not found.
 */
int
#ifdef CK_ANSIC
lkup(char *name)
#else
lkup(name) char *name;
#endif  /* CK_ANSIC */
{
    struct sym *cur;
    for (cur = htab[hash(name)]; cur != NULL; cur = cur->hnxt)
      if (strcmp(cur->name,name) == 0) return(cur->val);
    return(-1);
}
