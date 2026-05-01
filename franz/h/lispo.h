/* lispo.h -- stub for the Linux x86_64 port of Franz Lisp.
 *
 * Originally a symlink to /usr/include/a.out.h alongside aout.h.
 * On Linux the real header doesn't exist; we provide the same stub
 * struct shapes via aout.h and route lispo.h through it so all
 * downstream includes resolve.
 *
 * See aout.h for rationale.
 */

#ifndef LISPO_STUB_H
#define LISPO_STUB_H

#include "aout.h"

#endif /* LISPO_STUB_H */
