# franz/macos_x86_64/Makefile -- build directory for the macos_x86_64 target.
#
# All targets share one set of per-arch sources and one set of build
# rules, both in ../linux_x86_64/ (the port's first platform, hence the
# name). This directory only holds the objects compiled from those
# sources and the linked rawlisp, so each target's build has its own
# name. Configure with ./lispconf; see BUILD.md.

ArchDir = ../linux_x86_64
VPATH   = $(ArchDir)
include $(ArchDir)/Makefile
