### 
### I think it's not worth to make such a small project
### modular. So this is a simple gnu Makefile...
###

.DELETE_ON_ERROR:
.PHONY: install clean all

GZIP := gzip
GZIPFLAGS := --best --to-stdout

all: fbgrab fbgrab.1.gz

fbgrab: fbgrab.c
	$(CC) -g -Wall $(CFLAGS) $(LDFLAGS) $< -lpng -lz -o $@

fbgrab.1.gz: fbgrab.1.man
	$(GZIP) $(GZIPFLAGS) $< > $@

install: fbgrab fbgrab.1.gz
	install -D -m 0755 fbgrab $(DESTDIR)/usr/bin/fbgrab
	install -D -m 0644 fbgrab.1.gz $(DESTDIR)/usr/share/man/man1/fbgrab.1.gz

clean:
	-$(RM) fbgrab fbgrab.1.gz *~ \#*\#
