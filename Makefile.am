SUBDIRS = ac doc examples libsylph po

EXTRA_DIST = config.rpath  \
	INSTALL.ja \
	README.ja \
	autogen.sh \
	makewin32.sh \
	mime.types

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	BZIP2=$(BZIP2_ENV) $(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

ACLOCAL_AMFLAGS = -I m4
