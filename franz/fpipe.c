#ifndef lint
static char *rcsid =
   "$Header: fpipe.c,v 1.3 85/05/22 07:53:41 sklower Exp $";
#endif


/*					-[Sat Jan 29 12:44:16 1983 by jkf]-
 * 	fpipe.c				$Locker:  $
 * pipe creation
 *
 * (c) copyright 1982, Regents of the University of California
 */


#include "global.h"
#include <signal.h>

FILE *fpipe(info)
FILE *info[2];
{
	register FILE *p;
	int fd[2];

	if(0 > pipe(fd)) return( (FILE *) -1);

	if(NULL==(p = fdopen(fd[0],"r"))) {
		close(fd[0]);
		close(fd[1]);
		return( (FILE *) -1);
	}
	info[0] = p;
	if(NULL==(p = fdopen(fd[1],"w"))) {
		close(fd[0]);
		close(fd[1]);
		return( (FILE *) -1);
	}
	info[1] = p;

	return((FILE *) 2); /*indicate sucess*/
}
/* Nioreset *************************************************************/

#ifdef RTPORTS
static lispval NiorUtil();    /* defined further below */
#endif

lispval
Nioreset() {
#ifndef	RTPORTS
	register FILE *p;

	for(p = &_iob[3]; p < _iob + _NFILE; p++) {
		if(p->_flag & (_IOWRT | _IOREAD)) fclose(p);
		}
#else	/* RTPORTS */
	/* NiorUtil is declared static below; _fwalk is glibc-specific
	 * (was BSD stdio). On Linux there's no equivalent loop -- fclose
	 * on individual ports is handled per-port. Stub for now.
	 */
#if linux_x86_64
	(void)NiorUtil;
#else
	_fwalk(NiorUtil);
#endif
#endif	/* RTPORTS */
	return(nil);
}

#ifdef RTPORTS
FILE FILEdummy;

static lispval
NiorUtil(p)
FILE *p;
{
	lispval handy;
	if(p==stdin||p==stdout||p==stderr)
		return(0);
	fclose(p);
	handy = P(p);
	if(TYPE(handy)==PORT) {
		handy->p = &FILEdummy;
	}
	return(nil);
}
FILE **xports;

#define LOTS (LBPG/(sizeof (FILE *)))
lispval P(p)
FILE *p;
{
	register FILE **q;
	extern int fakettsize;

	if(xports==((FILE **) 0)) {
		/* this is gross.  I don't want to change csegment -- kls */
		xports = (FILE **) csegment(OTHER,LOTS,0);
		SETTYPE(xports,PORT,31);
		for(q = xports; q < xports + LOTS; q++) {
			*q = &FILEdummy;
		}
	}
	for(q = xports; q < xports + LOTS; q++) {
		if(*q==p) return ((lispval)q);
		if(*q==&FILEdummy) {
			*q = p;
			return ((lispval)q);
		}
	}
	/* Heavens above knows this could be disasterous in makevals() */
	error("Ran out of Ports",FALSE);
}

#endif	RTPORTS

FILE *
fstopen(base,count,flag)
char *base;
char *flag;
{
#if linux_x86_64
	/* On glibc the right tool is fmemopen(3): create a FILE*
	 * backed by a caller-supplied memory buffer. The original
	 * fstopen built one by hand by poking BSD FILE internals
	 * (the torek_stdio branch below was for ChrisTorek's stdio,
	 * and the #else branch for SysV stdio); both peek at fields
	 * glibc treats as private.
	 */
	return fmemopen(base, count, flag);
#else
	register FILE *p = fdopen(0,flag);

#ifdef torek_stdio
	p->_flags |= __SSTR;
	if(flag[0] == 'r')
	    p->_r = p->_bf._size = count;
	else
	    p->_w = p->_bf._size = count;
	p->_p = p->_bf._base = base;
	p->_file = -1;
#else
	p->_flag |= _IOSTRG;
	p->_cnt = count;
	p->_ptr = p->_base = base;
	p->_file = -1;
#endif
	return(p);
#endif /* linux_x86_64 */
}

#ifdef SPISFP
#ifndef i386_4_3
char *
alloca(howmuch)
register int howmuch;
{
	howmuch += 3 ;
	howmuch >>= 2;
	xsp -= howmuch;
	if (xsp < xstack) {
		xsp += howmuch;
		xserr();
	}
	return((char *) xsp);
}
#endif
#endif
