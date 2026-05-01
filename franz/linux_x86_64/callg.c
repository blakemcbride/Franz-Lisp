/*
 * franz/linux_x86_64/callg.c
 *
 * callg_ -- trampoline for "call this C function pointer with N args
 * from this arglist". Used by Ifcall (eval2.c) for foreign-function
 * calls and by Lsyscall (fex4.c) for syscall(2).
 *
 *   arglist[0] = N (number of args)
 *   arglist[1..N] = the N args, each long-sized (holds an int, a
 *                   pointer, or an indirect pointer to a boxed value)
 *
 * The original i386 implementation (see reference/i386/callg.c) used
 * a gcc-on-i386 trick: declare a VLA, copy args into it, then call
 * (*fn)() with no explicit args -- exploiting the cdecl convention
 * where args are pushed onto the C stack. On x86_64 SysV that doesn't
 * work: the first 6 integer args go in rdi/rsi/rdx/rcx/r8/r9, and
 * additional args are pushed only after stack realignment. There's no
 * way to "smuggle" args via stack memory.
 *
 * Approach: dispatch on the arg count and do direct C calls. The
 * compiler generates correct register loading for each fixed-arity
 * call. Up to 8 integer/pointer args are handled (covers all
 * documented Franz Lisp foreign-call patterns and syscall(2), which
 * tops out at 6 args on Linux). Anything beyond errors out at runtime.
 *
 * Limitations vs. the original (any of which can be lifted later):
 *   - Integer/pointer args only. Doubles passed via callg_ would
 *     need to land in xmm0..xmm7 instead of integer registers; the
 *     'r'/'d' branches in Ifcall are aware of this and use a separate
 *     trampoline (callg_d below).
 *   - VECTORI structure arguments (`v` discipline in Ifcall) aren't
 *     supported. Fixing this requires synthesizing arbitrary stack
 *     layouts, i.e. libffi or hand-rolled asm.
 */

#include "global.h"

extern lispval error();

long
callg_(fn, arglist)
long (*fn)();
long *arglist;
{
    long n = arglist[0];
    long *a = arglist + 1;

    switch (n) {
    case 0: return fn();
    case 1: return fn(a[0]);
    case 2: return fn(a[0], a[1]);
    case 3: return fn(a[0], a[1], a[2]);
    case 4: return fn(a[0], a[1], a[2], a[3]);
    case 5: return fn(a[0], a[1], a[2], a[3], a[4]);
    case 6: return fn(a[0], a[1], a[2], a[3], a[4], a[5]);
    case 7: return fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    case 8: return fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    default:
        error("callg_: too many args for the x86_64 trampoline (max 8)",
              FALSE);
        return 0;
    }
}

/* callg_d -- variant that returns a double. On x86_64 SysV the
 * floating-point return value lives in xmm0, not rax, so we need a
 * separately-typed entry point; casting `callg_` to a double-returning
 * function pointer would read xmm0 (garbage) instead of rax.
 *
 * Args are still integer/pointer-passed; only the *return* type
 * differs. (Foreign functions that take double args would also need
 * xmm-register loading; not supported yet.)
 */
double
callg_d(fn, arglist)
double (*fn)();
long *arglist;
{
    long n = arglist[0];
    long *a = arglist + 1;

    switch (n) {
    case 0: return fn();
    case 1: return fn(a[0]);
    case 2: return fn(a[0], a[1]);
    case 3: return fn(a[0], a[1], a[2]);
    case 4: return fn(a[0], a[1], a[2], a[3]);
    case 5: return fn(a[0], a[1], a[2], a[3], a[4]);
    case 6: return fn(a[0], a[1], a[2], a[3], a[4], a[5]);
    case 7: return fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    case 8: return fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    default:
        error("callg_d: too many args for the x86_64 trampoline (max 8)",
              FALSE);
        return 0.0;
    }
}
