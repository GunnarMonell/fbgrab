### 
### I think it's not worth to make such a small project
### modular. So this is a simple gnu Makefile...
###

fbgrab: fbgrab.c
	$(CC) -g -Wall fbgrab.c -lpng -lz -o fbgrab

install:
	install -D fbgrab $(DESTDIR)/usr/bin/fbgrab
	install -D fbgrab.1.man $(DESTDIR)/usr/man/man1/fbgrab.1

clean:
	rm -f fbgrab *~ \#*\#
