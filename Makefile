### 
### I think it's not worth to make such a small project
### modular. So this is a simple gnu Makefile...
###

fbgrab: fbgrab.c
#	splint +posixlib fbgrab.c
	gcc -g -Wall fbgrab.c -lpng -lz -o fbgrab

install:
#	strip fbgrab
	install fbgrab /usr/bin/fbgrab
	install fbgrab.1.man /usr/man/man1/fbgrab.1

clean:
	rm -f fbgrab *~ \#*\#