# $Header: Makefile 1.4 83/07/21 21:27:16 sklower Exp $
#	makefile for the franz lisp manual
#
# sources: lmacs: macros for the franz documents
#	   ch1.n  intro and description of types
#	   ch2.n  data structure access
#	   ch3.n  on arithmetic functions.
#	   ch4.n  special functions.
#	   ch5.n  i/o
#	   ch6.n  system functions
#	   ch7.n  reader
#	   ch8.n  functions and macros
#	   ch9.n  arrays
#	   ch10.n exception handling
#	   ch11.n trace package
#	   ch12.n liszt
#	   ch13.n cmu top level
#	   ch14.n stepper
#	   ch15.n fixit package
#	   ch16.n lisp editor
#	   chb.n special symbols
#	   chc.n  short subjects


.SUFFIXES: .n .t .x .v .r .rx .q .qx .sp

# the syntax's have this meaning:
#  .n  nroff/troff source file
#  .t  troff output file, can be vpr -t 'ed
#  .x  index file from a troff run, when collected and run through troff
#	again, an index is produced.
#  .v  this file never exists, but asking for it will cause a .t file to
#	be created and then vpr'ed.  the .t file will not be deleted.
#  .p  this file also never exists, but asking for it will cause TROFF
#	(usually vtroff or itroff) to be run directly on the file, leaving
#	no .t around.  This is used in /usr/doc for people who want to
#	run off a manual and are too lazy to read this makefile.
#  .r  nroff output file.
#  .rx  special index output from nroff run.  These files should be catted
#       together and then left around for lisp to read when given the help
#	command.
#
#  .q  nroff output file compatible with model 37
#  .qx index file for .q files.
#
#  .sp spell errors
#
# make install will install the nroff versions of the manual in the
# directory (LibDir/manual) where the auxfns0.l help command can find them.
#
# ----------------------------------------------------------------------------
# Linux x86_64 port note:
#   The original makefile shelled out to a tiny C helper (utils/append) to
#   turn a list of roots ("ch0 ch1 ...") into a list of suffixed file names
#   ("ch0.r ch1.r ..."), because 1980s make had no string functions.  GNU
#   make does this natively with $(addsuffix ...), so the helper -- and the
#   C compile of append.c that used to break `make' here -- is gone.
#   Typeset output now uses groff instead of the long-dead /usr/ucb/vtroff,
#   and LibDir points at the in-tree lisplib so `make install' drops the
#   help text where bin/lisp actually reads it.
# ----------------------------------------------------------------------------
#
LibDir = ../lisplib
CcodeDir = ../franz
CopyTo = /dev/null
TROFF=	groff
NROFF=  nroff
O =
Rmt =

#--- Sources:
#	We use the suffixes to tell make how to make a file.  Thus
#  we only specify the root and let $(addsuffix ...) add the
#  appropriate suffix.

MacroSrc = lmacs

RootGenSrc = ch0 ch1 ch2 ch3 ch4 ch5 ch6 ch61 ch7 ch8 \
      ch9 ch10 ch11 ch12 ch13 ch14 ch15 ch16 ch17 chb chc

UtilSrc = Makefile indexsed mantags extrnames.awk fixmks.sed \
	franz.n

# the suffixed file lists, derived from the roots above.
RootN  = $(addsuffix .n,$(RootGenSrc))
RootR  = $(addsuffix .r,$(RootGenSrc))
RootRX = $(addsuffix .rx,$(RootGenSrc))
RootT  = $(addsuffix .t,$(RootGenSrc))
RootV  = $(addsuffix .v,$(RootGenSrc))
RootP  = $(addsuffix .p,$(RootGenSrc))
RootQ  = $(addsuffix .q,$(RootGenSrc))
RootX  = $(addsuffix .x,$(RootGenSrc))


all:
	$(MAKE) rall

rall:
	$(MAKE) NROFF=$(NROFF) O=$(O) LibDir=$(LibDir) $(RootR) helpindex

vall:
	$(MAKE) TROFF=$(TROFF) O=$(O) $(RootV) index.v

tall:
	$(MAKE) TROFF=$(TROFF) O=$(O) $(RootT) index.t

cctall:
	$(MAKE) TROFF="groff -s12" tall

pall:
	$(MAKE) TROFF=$(TROFF) O=$(O) $(RootP) pindex

troff: pall

qall:
	$(MAKE) NROFF=$(NROFF) O=$(O) $(RootQ) index.t

spall:
	$(MAKE) TROFF=$(TROFF) O=$(O) $(RootQ) index.t

# only a few files describe functions which are indexed.

.t.v:
	vpr -t $*.t

.n.t:
	tbl lmacs $*.n | ${TROFF} -me ${O} -t 1> $*.t 2> $*.x

.n.p:
	tbl lmacs $*.n | ${TROFF} -me ${O} 2> $*.x

.n.x:
	tbl lmacs $*.n | ${TROFF} -me -z 2> $*.x

.n.r:
	tbl lmacs $*.n | ${NROFF} -rb3 -me ${O} 1> $*.r 2> $*.rx
	rm -f helpindex

.n.rx:
	tbl lmacs $*.n | ${NROFF} -rb3 -me 1> $*.r 2> $*.rx
	rm -f helpindex

.n.q:
	tbl lmacs $*.n | ${NROFF} -me -T37 2> $*.qx | col > $*.q

.n.sp:
	spell $*.n > $*.sp

install: rall
	cp $(RootR) helpindex $(LibDir)/manual

clean:
	-rm -f *.r
	-rm -f *.rx
	-rm -f helpindex
	-rm -f *.t
	-rm -f *.q
	-rm -f *.x


findex:
	$(MAKE) $(RootX)
	echo ".Ib" > index
	sort +3 -o index index $(RootX)
	sed -f indexsed index > indexx
	${Rmt} ${TROFF} -me lmacs indexx

index.t:
	$(MAKE) $(RootX)
	echo ".Ib" > index
	sort +3 -o index index $(RootX)
	sed -f indexsed index > indexx
	${TROFF} -me -x -t lmacs indexx > index.t

pindex:
	$(MAKE) $(RootX)
	echo ".Ib" > index
	sort +3 -o index index $(RootX)
	sed -f indexsed index > indexx
	${TROFF} -me lmacs indexx

helpindex:
	$(MAKE) $(RootRX)
	cat $(RootRX) | tr '\227' ' ' > helpindex

tags:	$(RootN)
	awk -f mantags $(RootN) | sort > tags


# to create a database for lxref to use:
C-database: ${CcodeDir}/sysat.c
	grep "^	MK" ${CcodeDir}/sysat.c > mks
	sed -f fixmks.sed < mks > mks.fixed
	(echo "(Chome)" ; cat mks.fixed ) > C-database
	rm -f mks mks.fixed

doc-database:
	awk -f extrnames.awk $(RootN) | sed -f fixmks.sed > doc-database


bigxref: C-database doc-database
	echo $(addprefix $(LibDir)/,$(shell cd $(LibDir) ; $(MAKE) echorequired)) | \
		sed 's/\.l/.x/g' > lisplibfiles
	lxref doc-database  C-database  `cat lisplibfiles` > bigxref

# simple table of contents, just a listing of which function is
# documented in which chapter
tofc:
	egrep "^.Lc|^.Lf|^.Lx|^.sh" $(RootN) > tofc


copysource:
	(tar cf - ${MacroSrc} $(RootN) ${UtilSrc} | \
	 (cd ${CopyTo} ; tar xf -))

scriptcatall:
	@(cd .. ; scriptcat doc doc ${MacroSrc} $(RootN) ${UtilSrc})
	@(cd .. ; scriptcat doc lisplib/manual \
	      $(RootR) helpindex)

copymanual: rall
	( cd ${FromDir}/manual ; \
	  cp $(RootR) helpindex ${CopyTo})
