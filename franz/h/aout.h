/* aout.h -- stub for the Linux x86_64 port of Franz Lisp.
 *
 * On the original BSD distribution this file was a symlink to
 * /usr/include/a.out.h, used by fasl.c (loading compiled .o files at
 * runtime) and by fex3.c's Ndumplisp (writing the current Lisp image
 * back to disk as a new executable).
 *
 * Linux uses ELF, not a.out, and both fasl and dumplisp are deferred
 * in this port (see PortPlan.md). This stub provides just enough
 * struct shape to let fasl.c and fex3.c COMPILE; any code path that
 * actually reads or writes one of these structures should never run.
 *
 * If you find the kernel reaching code that uses a_magic, a_text, etc.
 * at runtime, that is a bug in the porting work, not in this header.
 */

#ifndef AOUT_STUB_H
#define AOUT_STUB_H

/* Placeholder a.out exec header. Fields named to satisfy references
 * in fasl.c (a_magic, a_text, a_data, a_syms) when reachable code
 * happens to compile them; runtime behaviour is undefined.
 */
struct exec {
    long a_magic;
    long a_text;
    long a_data;
    long a_bss;
    long a_syms;
    long a_entry;
    long a_trsize;
    long a_drsize;
};

/* Placeholder symbol-table entry. */
struct nlist {
    union {
        char *n_name;
        long  n_strx;
    } n_un;
    unsigned char n_type;
    char          n_other;
    short         n_desc;
    unsigned long n_value;
};

#endif /* AOUT_STUB_H */
