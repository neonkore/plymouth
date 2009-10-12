INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = bitmap.la

bitmap_la_CFLAGS = $(PLYMOUTH_CFLAGS)

bitmap_la_LDFLAGS = -module -avoid-version -export-dynamic
bitmap_la_LIBADD = $(PLYMOUTH_LIBS)                                         \
                  ../../../libply/libply.la                                \
                  ../../../libplybootsplash/libplybootsplash.la
bitmap_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
