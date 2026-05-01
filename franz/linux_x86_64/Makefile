# Makefile for the Linux x86_64 port of the Franz Lisp C kernel.
#
# This is the per-arch Makefile, invoked from the top-level Makefile
# as ${FranzD} where Mach=linux_x86_64. It is derived from
# franz/i386/Makefile but with the dual-arch and HOLE/rawhlisp variants
# removed -- on Linux there is exactly one supported configuration.
#
# Phase 0 of the port (see PortPlan.md): we only need this file to
# compile individual .o files. Linking rawlisp comes in Phase 2.

LibDir = /usr/local/src/franz/lisplib
ObjDir = /usr/local/src/franz/bin
CopyTo = /dev/null
Liszt  = liszt
Lisp   = lisp

# Modern gcc, with porting-friendly flags. We *want* warnings during
# Phase 1 but compilation must not fail on K&R-era issues:
#   -std=gnu89                   accept K&R function definitions
#   -fno-strict-aliasing         the Lisp value union casts heavily
#   -fno-omit-frame-pointer      easier gdb session in Phase 3
#   -no-pie -fno-pic             deterministic addresses while debugging
#   -Wno-implicit-function-declaration
#                                K&R callers don't prototype callees
#   -Wno-implicit-int            old-style "foo() {...}" returning int
#   -Wno-int-conversion          int<->pointer casts; we'll fix in Phase 1
#   -Wno-builtin-declaration-mismatch
#                                BSD-shape malloc/etc declared locally
CFLAGS = -m64 -O0 -g \
	-fno-strict-aliasing -fno-omit-frame-pointer \
	-no-pie -fno-pic \
	-std=gnu89 \
	-Wno-implicit-function-declaration \
	-Wno-implicit-int \
	-Wno-int-conversion \
	-Wno-builtin-declaration-mismatch \
	-I../h

# All C sources in the machine-independent kernel and this arch dir.
# (The .l/.o split, fasl, and rlc.c live in franz/ and franz/${Mach}/
#  respectively; for Phase 0 we only need .c -> .o.)
MipSrc = ../alloc.c ../data.c ../rlc.c \
	../lisp.c ../eval.c ../eval2.c ../inits.c ../io.c ../error.c \
	../sysat.c ../lam1.c ../lam2.c ../lam3.c ../lam4.c ../lam5.c \
	../lam6.c ../lam7.c ../lam8.c ../lam9.c ../lamr.c ../lamp.c \
	../fex1.c ../fex2.c ../fex3.c ../fex4.c ../fexr.c \
	../fpipe.c \
	../subbig.c ../pbignum.c ../divbig.c \
	../ffasl.c ../fasl.c \
	../trace.c ../evalf.c ../frame.c ../lamgc.c \
	../low.c

MipObj = ../alloc.o ../data.o ../rlc.o \
	../lisp.o ../eval.o ../eval2.o ../inits.o ../io.o ../error.o \
	../sysat.o ../lam1.o ../lam2.o ../lam3.o ../lam4.o ../lam5.o \
	../lam6.o ../lam7.o ../lam8.o ../lam9.o ../lamr.o ../lamp.o \
	../fex1.o ../fex2.o ../fex3.o ../fex4.o ../fexr.o \
	../fpipe.o \
	../subbig.o ../pbignum.o ../divbig.o \
	../ffasl.o ../fasl.o \
	../trace.o ../evalf.o ../frame.o ../lamgc.o \
	../low.o

# Pattern rule for kernel sources in ../ (parent dir).
../%.o: ../%.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Smoke target for Phase 0: compile lisp.c (a small file that builds
# cleanly under our Phase 0 flags). alloc.c hits real Phase 1 issues
# -- missing setbit/ftstbit macros for our arch and pointer-width casts
# -- so it isn't a valid smoke test.
smoke: ../lisp.o
	@echo "Phase 0 smoke compile: lisp.o produced."
	@file ../lisp.o

# Arch-specific sources (this directory). Currently just the callg
# trampoline; bignum/asm helpers will follow as Phase 1g/1h work.
ArchSrc = callg.c
ArchObj = callg.o

# Build a single arch-specific .o.
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Compile every machine-independent kernel source. Phase 1 milestone
# is when this target succeeds.
allkernel: $(MipObj) $(ArchObj)

clean:
	rm -f ../*.o *.o rawlisp nlisp snlisp

.PHONY: smoke allkernel clean
