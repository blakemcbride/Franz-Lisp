# $Header: Makefile,v 1.1 83/04/20 10:29:24 sklower Exp $
# $Locker: jkf $

LibDir = /usr/lib/lisp
AllSrc = Makefile append.c tackon.c

AllObj = ${LibDir}/append ${LibDir}/tackon

all: ${LibDir}/append ${LibDir}/tackon

${LibDir}/append: append.c
	cc -O -o ${LibDir}/append  -I../franz/h append.c

${LibDir}/tackon: tackon.c
	cc -O -o ${LibDir}/tackon -I../franz/h tackon.c

copysource: ${AllSrc}
	(tar cf - ${AllSrc} | (cd ${CopyTo} ; tar xf -))


scriptcatall: ${AllSrc}
	@../scriptcat . utils ${AllSrc}


# Only remove in-tree build products. (This used to `rm ${AllObj}`,
# i.e. installed binaries under /usr/lib/lisp, and failed whenever
# they didn't exist -- which is always, since nothing installs them.)
clean:
	rm -f append tackon *.o
