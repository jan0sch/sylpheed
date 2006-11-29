
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"LibSylph\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

INCLUDES = $(GLIB_CFLAGS) -I$(top_srcdir) -I$(includedir)

lib_LTLIBRARIES = libsylph.la
#noinst_LTLIBRARIES = libsylph.la

libsylph_la_SOURCES = \
	account.c \
	base64.c \
	codeconv.c \
	compose.c \
	customheader.c \
	displayheader.c \
	filter.c \
	folder.c \
	html.c \
	imap.c \
	mbox.c \
	md5.c \
	md5_hmac.c \
	mh.c \
	news.c \
	nntp.c \
	pop.c \
	prefs.c \
	prefs_account.c \
	prefs_common.c \
	procheader.c \
	procmime.c \
	procmsg.c \
	quoted-printable.c \
	recv.c \
	session.c \
	smtp.c \
	socket.c \
	ssl.c \
	stringtable.c \
	sylmain.c \
	unmime.c \
	utils.c \
	uuencode.c \
	virtual.c \
	xml.c

libsylphincludedir=$(includedir)/sylph
libsylphinclude_HEADERS = \
	defs.h \
	enums.h \
	account.h \
	base64.h \
	codeconv.h \
	compose.h \
	customheader.h \
	displayheader.h \
	filter.h \
	folder.h \
	html.h \
	imap.h \
	mbox.h \
	md5.h \
	md5_hmac.h \
	mh.h \
	news.h \
	nntp.h \
	pop.h \
	prefs.h \
	prefs_account.h \
	prefs_common.h \
	procheader.h \
	procmime.h \
	procmsg.h \
	quoted-printable.h \
	recv.h \
	session.h \
	smtp.h \
	socket.h \
	ssl.h \
	stringtable.h \
	sylmain.h \
	unmime.h \
	utils.h \
	uuencode.h \
	virtual.h \
	xml.h

if OS_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libsylph.def
endif

libsylph_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libsylph_la_LIBADD = $(GLIB_LIBS)
