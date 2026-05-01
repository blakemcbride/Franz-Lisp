/* clinkfns.c -- runtime link routines for compiled-to-C Lisp.
 *
 * Original by Jeff Dalton, AIAI, University of Edinburgh, 1986-87.
 * Copied from reference/cliszt-i386/clinkfns.c with int-to-long
 * widenings the rest of the kernel needed on x86_64 LP64.
 *
 * The compilers init() template (see cinit.c / the inline init in
 * liszts output) calls clink(literals, ...) which:
 *   - reads back the literal table by rdstr ing each printed-rep
 *     string into the corresponding lispval slot,
 *   - allocates and initializes a transfer table whose entries
 *     initially route through clinker until each call site is
 *     resolved on first hit (a la sstatus translink),
 *   - walks the bind table to either define a function (most slots)
 *     or eval a top-level form.
 *
 * Phase 2 of CompilerPlan.md: get this file building and linked into
 * rawlisp so the loaded shared object can call it. Phase 3 wires up
 * dlopen so the shared object actually gets loaded.
 */

#include <stddef.h>		/* size_t -- needed before glibc <stdio.h> */
#include "global.h"
#include "frame.h"
#include "structs.h"
#include "chkrtab.h"
#include "compiled.h"

#undef CL_DEBUG			/* define to get linker-size/trans-size traces */

extern lispval eval();
extern lispval Ipurcopy();
extern long *bind_lists;        /* widened from int* (see data.c) */
extern int uctolc;

struct trent *gettran();

static lispval rdstr();
static lispval *make_linktable();
lispval clinker();

static int clink_note_redef = 0;

lispval
set_redef()
{
	int previous = clink_note_redef;
	clink_note_redef = lbot->val != nil;
	return previous ? tatom : nil;
}


/* clink -- called by compiled Lisp's per-file init routine. */

clink(literals, linker_size, linkv, callnames, trans_size, transv, bindtab)
char *literals[];
int linker_size;
lispval **linkv;
char *callnames[];
int trans_size;
struct trent **transv;
struct bindspec *bindtab;
{
	struct trent *tranloc;
	int i;
	lispval v;
	lispval currtab,	/* current readtable and ibase -- must be */
	        curibase;	/* kept up to date as we evaluate forms */
	int ouctolc;		/* ditto uctolc flag */
	int note_redef = clink_note_redef;

	Savestack(0);

#ifdef CL_DEBUG
	printf("linker size = %d, trans size = %d\n", linker_size, trans_size);
#endif

	/* Use the standard read state for parsing literal-table strings,
	 * but remember + restore the user's settings around eval calls. */
	currtab = Vreadtable->a.clb;
	Vreadtable->a.clb = strtab;
	curibase = ibase->a.clb;
	ibase->a.clb = inewint(10);
	ouctolc = uctolc;
	uctolc = FALSE;

	PUSHDOWN(gcdis, tatom);		/* turn off gc */

	/* link table */
	if (linker_size > 0)
		*linkv = make_linktable(literals, linker_size);
	else
		*linkv = 0;

	/* transfer table */
	if (trans_size > 0)
	{
		tranloc = gettran(trans_size);
		*transv = tranloc;
		for (i = 0; i < trans_size; i++)
		{
			v = rdstr(callnames[i]);
			tranloc->name = v;
			tranloc->fcn = clinker;
			tranloc++;
		}
	}
	else
	{
		*transv = 0;
	}

	/* bind table -- per slot, either eval a form or define a function. */
	for (; bindtab->btype; bindtab++)
	{
		if (bindtab->btype == &(tatom))
		{
			/* eval top-level form */
			v = rdstr(bindtab->bname);
			protect(v);

			Vreadtable->a.clb = currtab;	/* restore for eval */
			ibase->a.clb = curibase;
			uctolc = ouctolc;
			eval(v);

			--np;				/* unprotect */

			if (uctolc) ouctolc = TRUE;	/* if eval changed */
			curibase = ibase->a.clb;	/*  these, remember */

			ibase->a.clb = inewint(10);	/* set back for next read */
			Vreadtable->a.clb = strtab;
			uctolc = FALSE;
		}
		else
		{
			/* define a function */
			v = rdstr(bindtab->bname);

			if (note_redef && (v->a.fnbnd != nil))
			{
				printr(v, stdout);
				printf(" redefined\n");
			}
			v = v->a.fnbnd = newfunct();
			v->bcd.start = bindtab->bentry;
			v->bcd.discipline = *bindtab->btype;
		}
	}

	/* Cleanup -- /\/ but what if there's an unwind?  this won't get done. */
	POP;

	Vreadtable->a.clb = currtab;
	chkrtab(currtab);
	ibase->a.clb = curibase;
	uctolc = ouctolc;

	Restorestack();
}


static lispval *
make_linktable(literals, link_size)
char *literals[];
int link_size;
{
	char *lc_org;			/* beginning of linker segment */
	char *lc_end;			/* last word in linker segment */
	char *lc_entries;		/* address of 1st lispval pointer */
	int i;
	long *iptr;			/* widened: writes pointer-sized slots */
	lispval *linktab;
	lispval handy;

	/* Linker table layout: [GC sentinel word][N lispval entries][-1 terminator].
	 * The +2*sizeof(long) accounts for the leading GC word and the
	 * trailing -1.
	 */
	lc_org = (char *)
	         csegment(OTHER,
	                  sizeof(lispval) * link_size + 2 * sizeof(long),
	                  TRUE);
	lc_end = lc_org + sizeof(lispval) * link_size + sizeof(long);
	lc_entries = lc_org + sizeof(long);

	/* Initialize entries to -1 so the GC's bind_lists walker stops at the end. */
	for (iptr = (long *)lc_entries; iptr <= (long *)lc_end; iptr++)
		*iptr = -1;

	/* Link our table into the GC's bind_lists chain unless the literals
	 * are going to be pure-copied (in which case the heap-allocated
	 * versions don't need to be tracked here).
	 */
	if (Vpurcopylits->a.clb == nil)
	{
		*(long *)lc_org = (long)bind_lists;	/* point to current */
		bind_lists = (long *)lc_entries;	/* point to first */
	}

	/* Convert each printed-rep literal into a lispval. */
	for (i = 0, linktab = (lispval *)lc_entries;
	     ((i < link_size) && (linktab < (lispval *)lc_end));
	     i++, linktab++)
	{
		handy = rdstr(literals[i]);
		if (Vpurcopylits->a.clb != nil)
			handy = Ipurcopy(handy);
		*linktab = handy;
	}

	if ((i != link_size) || (linktab != (lispval *)lc_end))
		error("clink: mismatch in linker table size", FALSE);

	return (lispval *)lc_entries;
}


/* rdstr -- read a single Lisp form from a C string. Adapted from
 * readlist (see lam6.c) by Jeff Dalton.
 *
 * /\/ Neither this routine nor readlist handle unterminated strings
 * correctly: input starting with """ or "|" loops if no matching
 * terminator is found. First sign is repeated GCs for vectori.
 */

extern FILE *fstopen();

static lispval
rdstr(string)
char *string;
{
	register lispval work;
	register FILE *p;
	lispval Lread();
	int count;
	pbuf pb;
	Savestack(2);

	count = strlen(string);
	p = fstopen(string, count, "r");

	/* Unwind-protect so we always free the port even if Lread errors. */
	errp = Pushframe(F_CATCH, Veruwpt, nil);
	switch (retval)
	{
	    lispval Lctcherr();
	case C_THROW:
		fclose(p);
		errp = Popframe();
		lbot = np;
		protect(lispretval->d.cdr);
		return Lctcherr();

	case C_INITIAL:
		lbot = np;
		protect(P(p));
		work = Lread();
		fclose(p);
		errp = Popframe();
		Restorestack();
		return work;
	}
	/* NOTREACHED */
	return nil;
}


/* clinker -- transfer-table link routine. Each call site in compiled
 * code initially routes through clinker; on first invocation we look
 * up the target function and (if '(status translink)' is on) cache
 * its bcd.start in the transfer-table slot for direct calls thereafter.
 */

struct trent *trlink;		/* set by compiled call sites */

lispval
clinker()
{
	register lispval fnb;

	if (Strans == (lispval)TRUE
	    && TYPE(fnb = trlink->name->a.fnbnd) == BCD
	    && TYPE(fnb->bcd.discipline) != STRNG)
	{
		trlink->fcn = fnb->bcd.start;
		return (*fnb->bcd.start)();
	}

	return Ifuncal(trlink->name);
}
