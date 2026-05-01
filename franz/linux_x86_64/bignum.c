/*
 * franz/linux_x86_64/bignum.c
 *
 * Bignum primitives for the Linux x86_64 port. Originally these were
 * one-function-per-file in franz/i386/ (preserved at reference/i386/
 * for cross-checking). Consolidated here because the structure is
 * uniform: each kernel works on arrays of 30-bit halfwords stored in
 * 32-bit ints, with carries flowing through a `struct vl { long high,
 * low; }`. The two helpers `emul` (32x32+32 -> 64) and `ediv` (64/32
 * -> quot+rem) abstract the multiply/divide operations that were
 * implemented in i386 asm in the original.
 *
 * Port notes:
 *   - The 30-bit-halfword scheme is preserved as-is; it works on
 *     x86_64 because `int` is still 32 bits. Phase 1h (separate from
 *     Phase 2) may later widen the scheme to 32-bit halfwords in
 *     int64_t/__int128 for ~2x throughput.
 *   - emul/ediv use `long long` for the 64-bit intermediate, which is
 *     guaranteed >= 64 bits and correct on any modern compiler.
 *   - Function bodies copied from reference/i386/ with K&R parameter
 *     declarations preserved so they continue to match the existing
 *     kernel callers without prototypes.
 */

#include "global.h"

struct vl { long high; long low; };

/* emul -- compute (p * q) + r as a 64-bit value, split into high and
 * low halves. Both p and q are nominally 30-bit halfwords (so the
 * product fits in 60 bits + 30 = 61 bits -- well under 64).
 *
 * Important: the 2-slot output array is `long s[2]`, not `int s[2]`,
 * because every caller in this file declares its result buffer as
 * `long res[2]` or `struct vl { long high, low; }`. On 32-bit those
 * widths happened to coincide; on x86_64 LP64 they don't, and writing
 * 4-byte ints into 8-byte slots leaves the high half undefined. The
 * downstream comparisons (`res[1] < 0`, `res[1] & 0x40000000`) need
 * the slot width to be consistent.
 */
void
emul(p, q, r, s)
int p, q, r;
long s[2];
{
    long long l = (long long)p * (long long)q + (long long)r;
    s[0] = (long)(l >> 32);
    s[1] = (long)(int)(l & 0xffffffffLL);
}

/* ediv -- divide a 64-bit value (held in p[0]:p[1] high:low) by q.
 * Returns the quotient; writes the remainder to p[0]. (The original
 * i386 ediv.s also wrote the quotient to p[1]; preserved for
 * compatibility with any caller that reads it post-call.)
 *
 * `err` was an overflow flag in the i386 version; we always set it
 * to 0 (no overflow) since the input range used by Franz Lisp keeps
 * the dividend under 2^61. p[0]/p[1] widths match emul's output --
 * `long`, not `int`, on x86_64.
 */
long
ediv(p, q, err)
long p[2];
long q;
char *err;
{
    long long dividend = ((long long)(unsigned int)p[0] << 32)
                       | (unsigned int)p[1];
    long long quot = dividend / q;
    long long rem  = dividend % q;
    p[0] = (long)(int)rem;
    p[1] = (long)(int)quot;
    if (err) *err = 0;
    return (long)quot;
}

/* dsmult -- multiply array of longs (top through bot inclusive) by
 * `mul`. Caller must have left space at top for the carry-out of the
 * most significant limb.
 */
dsmult(top, bot, mul)
long *top, *bot, mul;
{
    register long *p;
    struct vl work;
    long add = 0;

    work.low = 0; work.high = 0;
    for (p = top; p >= bot; p--) {
        emul(*p, mul, add, &work);
        *p = work.low & 0x3fffffff;
        add = work.high;
        add <<= 2;
        if (work.low < 0) add += 2;
        if (work.low & 0x40000000) add += 1;
    }
    p[1] = work.low;
}

/* dodiv -- divide an array of base-10^9 limbs by 10^9, returning the
 * remainder. Used during decimal printing of bignums.
 */
long
dodiv(top, bottom)
long *top, *bottom;
{
    struct vl work;
    char err;
    long rem = 0;
    register long *p = bottom;

    for (; p <= top; p++) {
        emul(0x40000000, rem, *p, &work);
        *p = ediv(&work, 1000000000, &err);
        rem = work.high;
    }
    return rem;
}

/* dsneg -- negate an array of 30-bit limbs in place. */
long
dsneg(top, bottom)
long *top, *bottom;
{
    register long *p = top;
    register long carry = 0;
    register long digit;

    while (p >= bottom) {
        digit = carry - *p;
        if (digit < 0) carry = -2;
        else carry = 0;
        if (digit & 0x40000000) carry += 1;
        *p-- = digit & 0x3fffffff;
    }
    return 0;
}

/* mlsb -- "multiply and subtract": utop[i] -= nqhat * vtop[i] (mod
 * 2^30) propagating borrow. Used by Knuth division (algorithm D).
 */
long
mlsb(utop, ubot, vtop, nqhat)
register long *utop, *ubot, *vtop;
register long nqhat;
{
    register long handy, carry;
    struct vl work;

    for (carry = 0; utop >= ubot; utop--) {
        emul(nqhat, *--vtop, carry + *utop, &work);
        carry = work.high;
        handy = work.low;
        *utop = handy & 0x3fffffff;
        carry <<= 2;
        if (handy & 0x80000000) carry += 2;
        if (handy & 0x40000000) carry += 1;
    }
    return carry;
}

/* adback -- "add back" a borrowed-too-much divisor: utop[i] += vtop[i]. */
long
adback(utop, ubot, vtop)
register long *utop, *ubot, *vtop;
{
    register long handy, carry;
    carry = 0;
    for (; utop >= ubot; utop--) {
        carry += *--vtop;
        carry += *utop;
        *utop = carry & 0x3fffffff;
        handy = carry;
        carry = 0;
        if (handy & 0x80000000) carry -= 2;
        if (handy & 0x40000000) carry += 1;
    }
    return carry;
}

/* dsdiv -- divide-by-single-digit through array of base-2^30 limbs. */
long
dsdiv(top, bot, div)
register long *top, *bot;
register long div;
{
    struct vl work;
    char err;
    register long handy, carry = 0;

    for (carry = 0; bot <= top; bot++) {
        handy = *bot;
        if (carry & 1) handy |= 0x40000000;
        if (carry & 2) handy |= 0x80000000;
        carry >>= 2;
        work.low = handy;
        work.high = carry;
        *bot = ediv(&work, div, &err);
        carry = work.high;
    }
    return carry;
}

/* dsadd1 -- add 1 to a bignum's least significant limb, propagating. */
dsadd1(top, bot)
long *top, *bot;
{
    register long *p, work, carry = 0;

    work = 0;
    for (p = top; p >= bot; p--) {
        work = *p + carry;
        *p = work & 0x3fffffff;
        carry = 0;
        if (work & 0x40000000) carry += 1;
        if (work & 0x80000000) carry -= 2;
    }
    p[1] = work;
}

/* dsrsh -- right-shift a bignum array by ncnt bits with mask. */
long
dsrsh(top, bot, ncnt, mask1)
long *top, *bot;
long ncnt, mask1;
{
    register long *p = bot;
    register long r = -ncnt, l = 30 + ncnt, carry = 0, work, save;
    long mask = -1 ^ mask1;

    while (p <= top) {
        save = work = *p;
        save &= mask;
        work >>= r;
        carry <<= l;
        work |= carry;
        *p++ = work;
        carry = save;
    }
    return carry;
}

/* calqhat -- Knuth Vol. 2 alg. D step D3: compute trial quotient
 * digit qhat = floor((u[j]*b + u[j+1]) / v[1]).
 */
calqhat(uj, v1)
register long *uj, *v1;
{
    struct vl work1, work2;
    register long handy, handy2;
    register long qhat, rhat;
    char err;

    if (*v1 == *uj) {
        qhat = 0x3fffffff;
        rhat = uj[1] + *v1;
    } else {
        handy2 = uj[1];
        handy = *uj;
        if (handy & 1) handy2 |= 0x40000000;
        if (handy & 2) handy2 |= 0x80000000;
        handy >>= 2;
        work1.low = handy2; work1.high = handy;
        qhat = ediv(&work1, *v1, &err);
        rhat = work1.high;
    }
again:
    emul(qhat, v1[1], 0, &work1);
    handy2 = uj[2]; handy = rhat;
    if (handy & 1) handy2 |= 0x40000000;
    if (handy & 2) handy2 |= 0x80000000;
    handy >>= 2;
    work2.low = handy2; work2.high = handy;
    /* vlsub(work1, work2): subtract work2 from work1 in place */
    work1.high -= work2.high;
    if (work2.low > work1.low) work1.high--;
    work1.low -= work2.low;

    if (work1.high <= 0) return qhat;
    qhat--; rhat += *v1;
    goto again;
}

/* exarith -- "exact arithmetic": (hi, lo) = mul1 * mul2 + add. */
long
exarith(mul1, mul2, add, hi, lo)
long mul1, mul2, add;
long *hi, *lo;
{
    struct vl work;
    register long rlo;

    emul(mul1, mul2, add, &work);
    add = work.high;
    add <<= 2;
    if ((rlo = work.low) < 0) add += 2;
    if (rlo & 0x40000000) add += 1;
    *lo = rlo & 0x3fffffff;
    *hi = add;
    if ((add == 0) || (add != -1)) return add;
    *lo = rlo;
    return 0;
}

/* dmlad -- "double-multiply-add" with 30-bit-limb conversion. */
#define SNIL 0
int big_debug = FALSE;

dmlad(ptr, mul, add)
lispval ptr;
long mul, add;
{
    lispval psave, penult;
    long c, res[2];

    psave = ptr;
    c = add;
    do {
        emul((ptr->s.I), mul, c, res);
        res[0] <<= 2;
        if (res[1] < 0) res[0] += 2;
        if (res[1] & 0x40000000) res[0] += 1;
        res[1] &= 0x3fffffff;
        c = res[0];
        ptr->s.I = res[1];
        penult = ptr;
        ptr = ptr->s.CDR;
    } while (ptr != (lispval)SNIL);

    if (c != 0) {
        if (c == -1)
            penult->s.I |= 0xc0000000;
        else {
            ptr = penult->s.CDR = newdot();
            ptr->s.I = c;
        }
    }
}

/* adbig -- bignum addition via stack-allocated limb arrays. */
struct s_dot { long I; struct s_dot *CDR; };
extern struct s_dot *export();

struct s_dot *
adbig(a, b)
struct s_dot *a, *b;
{
    int la = 1, lb = 1;
    long *sa, *sb, *sc, *base;
    register struct s_dot *p;
    register long *q, *r, *s;
    register long carry = 0;
    Keepxs();

    for (p = a; p->CDR; p = p->CDR) la++;
    for (p = b; p->CDR; p = p->CDR) lb++;
    if (lb > la) la = lb;

    base = (long *)alloca((3 * la + 1) * sizeof(long));
    sc = base + la + 1;
    sb = sc + la;
    sa = sb + la;
    q = sa;

    p = a;
    do { *--q = p->I; p = p->CDR; } while (p);
    while (q > sb) *--q = 0;
    p = b;
    do { *--q = p->I; p = p->CDR; } while (p);
    while (q > sc) *--q = 0;

    for (q = sa, r = sb, s = sc; q > sb; ) {
        carry += *--q + *--r;
        *--s = carry & 0x3fffffff;
        carry >>= 30;
    }
    *--s = carry;

    p = export(sc, base);
    Freexs();
    return p;
}

/* mulbig -- bignum multiplication. */
struct s_dot *
mulbig(a, b)
struct s_dot *a, *b;
{
    int la = 1, lb = 1;
    long *sa, *sb, *sc, *base;
    register struct s_dot *p;
    register long *q, *r, *s;
    long carry = 0, test;
    struct vl work;
    Keepxs();

    for (p = a; p->CDR; p = p->CDR) la++;
    for (p = b; p->CDR; p = p->CDR) lb++;

    base = (long *)alloca((la + la + lb + lb + 1) * sizeof(long));
    sc = base + la + lb + 1;
    sb = sc + lb;
    sa = sb + la;
    q = sa;

    p = a;
    do { *--q = p->I; p = p->CDR; } while (p);
    p = b;
    do { *--q = p->I; p = p->CDR; } while (p);
    while (q > base) *--q = 0;

    for (q = sb; q > sc; *--s = carry)
        for ((r = sa, s = (q--) - lb, carry = 0); r > sb; ) {
            carry += *--s;
            emul(*q, *--r, carry, &work);
            test = work.low;
            carry = work.high << 2;
            if (test < 0) carry += 2;
            if (test & 0x40000000) carry += 1;
            *s = test & 0x3fffffff;
        }

    p = export(sc, base);
    Freexs();
    return p;
}

/* mmuladd -- (a*b + c) mod m. Used by random-number primitive Lhrand. */
long
mmuladd(a, b, c, m)
long a, b, c, m;
{
    long work[2];
    char err;
    emul(a, b, c, work);
    ediv(work, m, &err);
    return work[0];
}

/* Imuldiv -- compute quotient and remainder of (p1*p2+add)/dv. */
Imuldiv(p1, p2, add, dv, quo, rem)
long p1, p2, add, dv;
long *quo, *rem;
{
    long work[2];
    char err;
    emul(p1, p2, add, work);
    *quo = ediv(work, dv, &err);
    *rem = *work;
}
