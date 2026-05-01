#ifndef lint
static char *rcsid =
   "$Header: rlc.c,v 1.5 87/12/14 17:19:20 sklower Exp $";
#endif

/*					-[Sat Jan 29 13:32:26 1983 by jkf]-
 * 	rlc.c				$Locker:  $
 * relocator for data space 
 *
 * (c) copyright 1982, Regents of the University of California
 */

#define TRUE 1
#include "h/global.h"

/* The data-segment relocator only matters for the HOLE variant of
 * Lisp (rawhlisp), which we do not build on Linux. The function is
 * referenced from a few places in the kernel; stub it as a no-op so
 * those references resolve at link time without dragging in the
 * BSD-syscall plumbing the original needed.
 */
rlc()
{
}
