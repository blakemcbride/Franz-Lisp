/*
 * franz/linux_x86_64/arch.c
 *
 * Arch-specific glue for the x86_64 port:
 *   - Small kernel helpers (inewint, blzero, prunei).
 *   - Non-local-jump frames under PORTABLE_FRAME (Ipushf, retframe,
 *     jmpval, qretfromfr).
 *   - The tynames[] table referenced by compiled-Lisp dispatch.
 *   - Stubs for things the original i386 port supplied via asm/kernel
 *     hacks that don't translate to x86_64: stack-walking debuggers,
 *     varargs nargs(), polynomial-eval, the fasl C-link entry.
 *
 * The bignum kernels live in bignum.c; the foreign-call trampoline
 * in callg.c.
 */

#include "global.h"
#include "frame.h"
#include "structs.h"

extern struct types int_str;
/* int_items, str_name, hunk_name etc. are all macros defined in
 * sigtab.h that expand to lispsys[N]; including global.h above pulls
 * them in. No extra extern decls needed.
 */

/* ------------------------------------------------------------------ */
/* inewint / blzero -- from reference/i386/inewint.c, types adjusted
 * to match the kernel-wide declarations in global.h/dfuncs.h.
 */

lispval
inewint(n)
long n;
{
    register lispval ip;
    if (n < 1024 && n >= -1024) return (lispval)(Fixzero + n);
    ip = newint();
    ip->i = n;
    return ip;
}

blzero(where, howmuch)
char *where;
long howmuch;
{
    register char *p;
    for (p = where + howmuch; p > where; ) *--p = 0;
}

/* ------------------------------------------------------------------ */
/* prunei -- return an INT cell to the free list. The original used a
 * gstart() heuristic ("address > code segment") to avoid pruning the
 * Fixzero[] static cells. On x86_64 with the mmap'd Lisp heap, a more
 * direct check is "address is inside the heap region".
 */

extern uintptr_t OFFSET;
extern lispval datalim;

prunei(what)
register lispval what;
{
    if ((uintptr_t)what >= OFFSET && (uintptr_t)what < (uintptr_t)datalim) {
        --(int_items->i);
        what->i = (long)int_str.next_free;
        int_str.next_free = (char *)what;
    }
}

/* gstart -- placeholder. Was the start-of-text-segment in the i386
 * a.out layout. Not meaningful with our mmap'd heap; only fex3
 * (dumplisp, stubbed for linux_x86_64) and prunei (rewritten above)
 * cared about it. Returning a low address satisfies code that does
 * "ptr > gstart" (false) checks safely.
 */
void *
gstart()
{
    return (void *)0;
}

/* ------------------------------------------------------------------ */
/* PORTABLE_FRAME globals + Ipushf + qretfromfr (from reference/i386/  */
/* qfuncl.c). With PORTABLE_FRAME defined, frame setup is just         */
/* setjmp; the machine-specific register save/restore is unneeded.    */

struct frame *retframe;       /* result of Pushframe (set by macro) */
int          jmpval;          /* setjmp return value, debug only */
char _erthrow[] = "Uncaught throw from compiled code";

struct frame *
Ipushf(fclass, arg1, arg2, loc_frame)
int fclass;
lispval arg1, arg2;
struct frame *loc_frame;
{
    loc_frame->olderrp = errp;
    loc_frame->svlbot  = lbot;
    loc_frame->svnp    = np;
    loc_frame->svbnp   = bnp;
#ifdef SPISFP
    loc_frame->svxsp   = xsp;
#endif
    loc_frame->class   = fclass;
    loc_frame->larg1   = arg1;
    loc_frame->larg2   = arg2;
    retval = C_INITIAL;
    return loc_frame;
}

qretfromfr(loc_frame)
struct frame *loc_frame;
{
    lbot = loc_frame->svlbot;
    np   = loc_frame->svnp;
    bnp  = loc_frame->svbnp;
#ifdef SPISFP
    xsp  = loc_frame->svxsp;
#endif
    retframe = loc_frame;
    longjmp(loc_frame->retenv, 1);
    /* NOT REACHED */
}

/* tynames -- maps each TYPE() value to a pointer to its print-name.
 * Referenced by compiled-Lisp dispatch and by some debugging code.
 */
lispval *tynames[] = {
    (lispval *)0,         /* UNBO */
    &str_name,            /* STRNG */
    &atom_name,           /* ATOM */
    &int_name,            /* INT */
    &dtpr_name,           /* DTPR */
    &doub_name,           /* DOUB */
    &funct_name,          /* BCD */
    &port_name,           /* PORT */
    &array_name,          /* ARRAY */
    &other_name,          /* OTHER */
    &sdot_name,           /* SDOT */
    &val_name,            /* VALUE */
    &hunk_name[0],        /* HUNK2 */
    &hunk_name[1],
    &hunk_name[2],
    &hunk_name[3],
    &hunk_name[4],
    &hunk_name[5],
    &hunk_name[6],
    &vect_name,
    &vecti_name
};

/* ------------------------------------------------------------------ */
/* Globals supplied by the deleted lowaux.s. holbeg/holend defined the
 * "hole" region in HOLE-variant builds; we don't build that variant
 * but a few references survive in the kernel and need a definition.
 */
char holbeg[1] = {0};
char holend[1] = {0};

/* ------------------------------------------------------------------ */
/* Stubs for things that don't translate cleanly to x86_64 or that
 * depend on machinery we've deferred (fasl, dumplisp, stack-walking).
 * Each errors at runtime if reached.
 */

extern lispval error();

/* nargs -- on i386 this read the call site's stack-adjustment
 * instruction to recover the caller's arg count. There's no analogue
 * on x86_64; return 0 so callers see "no args" rather than crashing.
 * Affects dumpmydata (a debug dumper) and ftolsp_ (C-to-Lisp callback,
 * which we don't exercise).
 */
nargs()
{
    return 0;
}

/* clinker -- the transfer-table link routine. Real implementation lives
 * in clinkfns.c (CompilerPlan Phase 2). The kernel sees it via this
 * extern; it's invoked when compiled code's transfer-table slots are
 * first hit.
 */
extern lispval clinker();

/* fasl/cfasl entry stubs -- referenced from sysat.c's function table
 * (Lcfasl, Lrmadd) and from fex3.c's Lgetaddress (dispget, gstab,
 * nlist). All belong to the foreign-function-loading machinery that
 * ffasl.c implements only on non-Linux targets. Each errors at
 * runtime if reached.
 */

/* Lcfasl -- load a compiled-Lisp shared object via dlopen/dlsym and
 * register one of its symbols as a Lisp function.
 *
 * Calling convention (matches the original BSD ffasl.c cfasl):
 *
 *   (cfasl filename csym atom discipline disc-arg)
 *
 *   filename  -- path to a .so (atom or string)
 *   csym      -- C symbol name to look up (atom or string, no leading
 *                underscore on Linux ELF)
 *   atom      -- Lisp atom whose function-binding is set to point at
 *                the looked-up address
 *   discipline-- e.g. "subroutine" or 'lambda; passed through to
 *                bcd.discipline so the eval/funcall machinery knows
 *                how to call into the function
 *   disc-arg  -- discipline argument (often the empty string)
 *
 * Returns t on success, nil on dlopen/dlsym failure.
 *
 * Implementation note: RTLD_LOCAL keeps each loaded .sos symbol table
 * private, so multiple compiled files can each have an `init` symbol
 * without colliding. Cross-file Lisp calls go through the transfer
 * table (clinker), not direct dlsym lookup.
 */

#include <dlfcn.h>

lispval
Lcfasl()
{
    void *handle, *addr;
    char *filename, *symname;
    lispval atomname, disc;
    lispval work;

    chkarg(5, "cfasl");

    filename = (char *) verify(lbot[0].val, "cfasl: bad filename");
    symname  = (char *) verify(lbot[1].val, "cfasl: bad C symbol name");

    if (TYPE(lbot[2].val) != ATOM)
        error("cfasl: third arg must be atom", FALSE);
    atomname = lbot[2].val;

    /* Discipline: pass through unmodified. The interpreter checks
     * whether bcd.discipline is a STRNG (foreign call) or an atom
     * like 'lambda (Lisp call) at invocation time.
     */
    disc = lbot[3].val;
    if (TYPE(disc) == ATOM)
        disc = (lispval) disc->a.pname;

    handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if (handle == (void *) 0) {
        fprintf(stderr, "cfasl: dlopen(\"%s\"): %s\n", filename, dlerror());
        return nil;
    }

    /* Clear any stale dlerror. */
    dlerror();
    addr = dlsym(handle, symname);
    {
        char *err = dlerror();
        if (err != (char *) 0) {
            fprintf(stderr, "cfasl: dlsym(\"%s\"): %s\n", symname, err);
            dlclose(handle);
            return nil;
        }
    }

    work = newfunct();
    work->bcd.start = (lispval (*) ()) addr;
    work->bcd.discipline = disc;
    atomname->a.fnbnd = work;

    /* Note: handle is intentionally not tracked here. dlclose'ing on
     * Lisp shutdown isn't critical, and dlclose during a session
     * would invalidate any Lisp atoms still bound to symbols from
     * that .so. A future cfasl-list could keep references for
     * introspection.
     */
    return tatom;
}

lispval
Lrmadd()
{
    return error("(rmadd) is not supported on linux_x86_64", FALSE);
}

lispval
dispget(given, msg, defult)
lispval given, defult;
char *msg;
{
    return error("dispget called -- foreign-function fasl unavailable",
                 FALSE);
}

char *
gstab()
{
    error("gstab called -- foreign-function fasl unavailable", FALSE);
    return (char *)0;
}

/* nlist -- BSD a.out symbol-table reader. fex3.c's Lgetaddress (which
 * resolves a C symbol name to its address by scanning the running
 * binary) calls it. Linux uses ELF and dlsym(); a faithful port would
 * use that. Stubbed here.
 */
nlist(name, table)
char *name;
struct nlist *table;
{
    return -1;       /* signals "symbol table unavailable" to caller */
}

/* Stack-walking helpers -- the original isho() walked the C call
 * stack via i386 frame layout (struct machframe), comparing return
 * addresses to known interpreter symbols (eval, Ifuncal, etc.) to
 * pick out evaluation frames. None of that translates to x86_64
 * frame-pointer chains directly, and a faithful port would need
 * libunwind. Stub for now.
 */
lispval
Lshostk()
{
    return error("(showstack) is not implemented on linux_x86_64", FALSE);
}

lispval
Lbaktrace()
{
    return error("(baktrace) is not implemented on linux_x86_64", FALSE);
}

lispval
LIshowstack()
{
    return error("(int:showstack) is not implemented on linux_x86_64", FALSE);
}

/* myfrexp / Lmkcth -- dummy stubs that were already dummies in the
 * i386 reference (see reference/i386/i386.c).
 */
myfrexp()      { return error("myfrexp called", FALSE); }
Lmkcth()       { fprintf(stderr, "mkcth called\n"); return 0; }

/* Lpolyev -- VAX polyd was a polynomial evaluator instruction. Not
 * available on x86_64 (or i386 originally; the reference also errored).
 */
lispval
Lpolyev()
{
    return error("polyev has not been implemented", FALSE);
}

/* Lrot -- rotate-bits. Pure C, ports cleanly. From reference/i386/i386.c. */
lispval
Lrot()
{
    register long val;
    register unsigned long mask2 = -1;
    register struct argent *mylbot = lbot;
    long rot;

    chkarg(2, "rot");
    if ((TYPE(mylbot->val) != INT) || (TYPE(mylbot[1].val) != INT))
        errorh2(Vermisc, "Non ints to rot",
                nil, FALSE, 0, mylbot->val, mylbot[1].val);
    val = mylbot[0].val->i;
    rot = mylbot[1].val->i;
    rot = rot & 0x3f;
    mask2 >>= rot;
    mask2 ^= -1;
    mask2 &= val;
    mask2 >>= (32 - rot);
    val <<= rot;
    val |= mask2;
    return inewint(val);
}
