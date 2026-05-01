# Top-level Makefile for the Linux x86_64 port of Franz Lisp.
#
# This file replaces the original 290-line multi-arch / distribution
# Makefile (see git history). It builds the C kernel via
# franz/${Mach}/Makefile and stages the per-phase work described in
# PortPlan.md.
#
# Targets currently meaningful:
#   make smoke      -- compile one kernel source (Phase 0 verification)
#   make kernel     -- compile every machine-independent kernel .c
#                      (Phase 1 milestone)
#   make rawlisp    -- link the raw kernel into an executable
#                      (Phase 2; currently fails at link time)
#   make clean
#
# Targets currently NOT meaningful (re-enabled in later phases):
#   make slow / fast / install
#       -- the original three-stage bootstrap that builds liszt, dumps
#          a Lisp image, etc. Re-enabled in Phase 5 once the interpreter
#          can boot and load library .l files at startup.

DESTDIR =
.DEFAULT: smoke

# Set by ./lispconf; only one platform is supported in this port.
Mach = linux_x86_64

RootDir = $(shell pwd)
LibDir  = ${DESTDIR}${RootDir}/lisplib
ObjDir  = ${DESTDIR}${RootDir}/bin

FranzD  = franz/${Mach}

# --- Phase targets ---

smoke:
	(cd ${FranzD} && $(MAKE) smoke)

kernel:
	(cd ${FranzD} && $(MAKE) allkernel)

rawlisp:
	(cd ${FranzD} && $(MAKE) rawlisp)

# Phase 5 placeholders.
slow fast install:
	@echo "Target '$@' is disabled. See PortPlan.md -- this work is in" ; \
	 echo "Phase 5 (replace dumplisp with library auto-load)."           ; \
	 exit 1

clean:
	(cd ${FranzD} && $(MAKE) clean)
	(cd utils     && $(MAKE) clean) || true
	rm -f franz/*.o

.PHONY: smoke kernel rawlisp slow fast install clean
