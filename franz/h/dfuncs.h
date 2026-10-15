/*					-[Sat Jan 29 13:54:30 1983 by jkf]-
 * 	dfuncs.h			$Locker:  $
 * external function declaration
 *
 * $Header: dfuncs.h,v 1.2 84/02/29 17:09:10 sklower Exp $
 *
 * (c) copyright 1982, Regents of the University of California
 */
 
char *brk();
char *getsp();
char *pinewstr();
char *inewstr();
char *mkmsg();
char *newstr();
char *rstore();
char *sbrk();
char *xsbrk();
char *ysbrk();
int csizeof();
int finterp();
lispval Iget();
lispval Imkrtab();
lispval Iputprop();
lispval Lfuncal();
lispval Lnegp();
lispval Lsub();
lispval alloc();
lispval copval();
lispval csegment();
lispval error();
lispval errorh();
lispval errorh1();
lispval errorh2();
lispval eval();
lispval gc();
lispval getatom();
lispval inewval();
lispval linterp();
lispval matom();
lispval mfun();
lispval mstr();
lispval newarray();
lispval newdot();
lispval newdoub();
lispval newfunct();
lispval newint();
lispval newsdot();
lispval newval();
lispval newhunk();
lispval pnewdot();
lispval pnewdb();
lispval pnewhunk();
lispval pnewint();
lispval pnewsdot();
lispval pnewval();
lispval popnames();
lispval r();
lispval ratomr();
lispval readr();
lispval readrx();
lispval readry();
lispval typred();
lispval unprot();
lispval verify();
struct atom * newatom();

/* Pointer- and long-returning helpers that some callers used without a
 * declaration. An implicit `int` return truncates them to 32 bits,
 * which goes unnoticed on Linux -no-pie (malloc/data sit below 4 GB)
 * but breaks on macOS, where every address is above 4 GB.
 */
char *atomtoolong();
FILE *fstopen();
lispval Ifuncal();
lispval Nioreset();
long callg_();
long dodiv();
long dsneg();
long exarith();
long mmuladd();
