# Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	"-DPACKAGE_LOCALE_DIR=\"$(localedir)\"" \
	"-DPACKAGE_SRC_DIR=\"$(srcdir)\"" \
	"-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"" \
	"-DG_LOG_DOMAIN=\"NautilusHide\""

AM_CFLAGS = \
	-Wall\
	-g \
	-pedantic \
	$(WARN_CFLAGS) \
	$(NAUTILUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(NAUTILUS_HIDE_CFLAGS)

nautilus_extensiondir = $(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES = \
	libnautilus-hide.la

libnautilus_hide_la_SOURCES = \
	nautilus-hide.c

libnautilus_hide_la_LDFLAGS = \
	-module \
	-avoid-version

libnautilus_hide_la_LIBADD = \
	$(NAUTILUS_HIDE_LIBS)

# EOF

