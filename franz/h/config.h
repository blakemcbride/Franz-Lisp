/* config.h -- Linux x86_64 port of Franz Lisp.
 *
 * Originally a multi-platform configuration file (vax/tahoe/68k/sun/
 * dual/pixel/mc500/lisa/i386/vms). All non-Linux platforms have been
 * stripped; see git history for the original.
 *
 *   Header in original distribution:
 *   $Header: config.h,v 1.16 87/12/14 18:33:31 sklower Exp $
 *   (c) copyright 1982, Regents of the University of California
 *   Modified by RMT 11 May 1992 for 386bsd
 *   Linux x86_64 port: 2026
 *
 * The lconf.h file (created by ./lispconf) #define's the target name,
 * e.g. `linux_x86_64 1' or `macos_arm64 1'. Every supported target
 * shares one configuration, the 64-bit Unix port, which the kernel
 * selects with the neutral macro `unix64' defined here.
 */

#include "lconf.h"

#if linux_x86_64 || linux_arm64 || macos_arm64 || macos_x86_64
#define unix64 1
#else
#error "lconf.h is not configured. Run ./lispconf (see ./lispconf -l)."
#endif


/* GCSTRINGS -- if defined, the GC reclaims strings. Off by default
 * because in typical usage the cost of collecting strings exceeds the
 * space recovered.
 */
/* #define GCSTRINGS */


/* ====================================================================
 * Machine identity
 * ==================================================================== */

#define m_64bit         1
/* m_64bit selects the 64-bit (LP64) code paths, on x86_64 and arm64
 * alike; MACHINE is what (status features) reports.
 */
#if defined(__aarch64__) || defined(__arm64__)
#define MACHINE         "arm64"
#else
#define MACHINE         "x86_64"
#endif

/* OFFSET is the base address subtracted from a Lisp pointer when
 * computing its page index in the type table (see ATOX in global.h).
 *
 * On the original i386 BSD port this was a compile-time `#define
 * OFFSET 0` because the heap started near address 0. Under Linux ASLR
 * no such guarantee holds; the heap is mmap'd at runtime and OFFSET
 * is a `uintptr_t` variable, declared `extern` in global.h and
 * defined in data.c. heap_init() (in alloc.c) sets it on first
 * allocation.
 */


/* ====================================================================
 * Operating system identity
 * ==================================================================== */

#define os_unix         1
#if macos_arm64 || macos_x86_64
#define os_macos        1
#else
#define os_linux        1
#endif
#define os_4_3          1   /* enables RTPORTS (runtime FILE* allocation) */
#define OS              "unix"

#define DOMAIN          "ucb"

/* SITE -- value of (sys:gethostname). On Linux this can be discovered
 * at runtime via gethostname(2), so leave it undefined here.
 */
/* #define SITE        "unknown-site" */


/* ====================================================================
 * Runtime feature flags
 * ==================================================================== */

/* PORTABLE         -- the Lisp value `nil` is &nilatom (a real address),
 *                     not the integer 0. Required when the heap can live
 *                     anywhere in the address space.
 *
 * PORTABLE_FRAME   -- non-local-jump frames use setjmp() rather than
 *                     hand-written register-saving asm.
 *
 * SPISFP           -- the Lisp argument-passing stack pointer is a
 *                     pure software variable, never the C stack pointer.
 *
 * RTPORTS          -- FILE* objects are allocated at runtime (true on
 *                     glibc; the alternative was the old _iob[] array).
 *
 * torek_stdio      -- use the getc/ungetc-based peekc() rather than
 *                     poking FILE internals directly. Required on glibc
 *                     where _cnt/_ptr are unavailable.
 */
#define PORTABLE        1
#define PORTABLE_FRAME  1
#define SPISFP          1
#define RTPORTS         1
#define torek_stdio     1

/* SIGTYPE -- return type of signal handlers. void on glibc. */
#define SIGTYPE         void

/* NILIS0 deliberately NOT defined: nil is &nilatom, not (lispval)0,
 * because the heap doesn't start at address 0 under ASLR. See PORTABLE
 * branches in global.h.
 */

/* NPINREG deliberately NOT defined: we don't keep np/lbot in dedicated
 * machine registers.
 */


/* ====================================================================
 * Heap sizing
 * ==================================================================== */

/* TTSIZE -- absolute limit, in LBPG-sized pages, on the size of the
 * Lisp heap. With LBPG=1024 on the 64-bit port, 32768 pages = 32 MB.
 * Original i386 default was 6120 (3 MB) with 512-byte pages.
 * Recompile alloc.c and data.c after changing.
 */
#define TTSIZE          32768
