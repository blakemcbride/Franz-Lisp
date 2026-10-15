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
#   make distclean  -- clean, plus ./lispconf's lconf.h, the manual
#                      build in doc/, compiled
#                      Lisp (.so) and lxref (.x) output anywhere in
#                      the tree, and editor/tool droppings. Leaves
#                      source edits alone.
#
# Targets currently NOT meaningful (re-enabled in later phases):
#   make slow / fast / install
#       -- the original three-stage bootstrap that builds liszt, dumps
#          a Lisp image, etc. Re-enabled in Phase 5 once the interpreter
#          can boot and load library .l files at startup.

DESTDIR =
.DEFAULT: smoke

# The target chosen by ./lispconf, read back from the lconf.h it wrote
# (`#define macos_arm64 1' -> macos_arm64). Each target builds in
# franz/<target>/; the shared per-arch sources live in franz/linux_x86_64/.
ArchDirs = linux_x86_64 linux_arm64 macos_arm64 macos_x86_64
Mach := $(shell sed -n 's/^\#define \([a-z0-9_]*\) 1$$/\1/p' franz/h/lconf.h 2>/dev/null)

RootDir = $(shell pwd)
LibDir  = ${DESTDIR}${RootDir}/lisplib
ObjDir  = ${DESTDIR}${RootDir}/bin

FranzD  = franz/${Mach}

# --- Phase targets ---

smoke: configured
	(cd ${FranzD} && $(MAKE) smoke)

kernel: configured
	(cd ${FranzD} && $(MAKE) allkernel)

rawlisp: configured
	(cd ${FranzD} && $(MAKE) rawlisp)

configured:
	@if [ -z "${Mach}" ] || [ ! -d "${FranzD}" ]; then \
	    echo "Not configured: run ./lispconf first (./lispconf -l lists targets)." >&2; \
	    exit 1; \
	fi

# Phase 5 placeholders.
slow fast install:
	@echo "Target '$@' is disabled. See PortPlan.md -- this work is in" ; \
	 echo "Phase 5 (replace dumplisp with library auto-load)."           ; \
	 exit 1

# Clean every target's build directory, so switching targets with
# ./lispconf never leaves stale objects behind.
clean:
	for d in ${ArchDirs}; do (cd franz/$$d && $(MAKE) clean) || exit 1; done
	(cd utils     && $(MAKE) clean) || true
	rm -f franz/*.o

# Remove everything a build can produce. find does not follow the
# `lisp' -> . and `liszt' -> cliszt symlinks, so nothing is visited twice.
distclean: clean
	(cd doc && $(MAKE) distclean)
	rm -f franz/h/lconf.h
	find . -path ./.git -prune -o \( -name '*.so' -o -name '*.dylib' \
	    -o -name '*.x' -o -name '*~' -o -name make.log \) -type f -print \
	    | xargs rm -f
	find . -path ./.git -prune -o -name '*.dSYM' -type d -print \
	    | xargs rm -rf

.PHONY: smoke kernel rawlisp configured slow fast install clean distclean
