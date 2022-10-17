AM_CPPFLAGS = -I$(top_srcdir)                                                 \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libply-splash-core                            \
           -I$(srcdir)/../../../libply-splash-graphics                        \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = label-pango.la

label_pango_la_CFLAGS = $(PLYMOUTH_CFLAGS) $(PANGO_CFLAGS)

label_pango_la_LDFLAGS = -module -avoid-version -export-dynamic
label_pango_la_LIBADD = $(PLYMOUTH_LIBS) $(PANGO_LIBS)                      \
                  ../../../libply/libply.la                                 \
                  ../../../libply-splash-core/libply-splash-core.la         \
                  ../../../libply-splash-graphics/libply-splash-graphics.la
label_pango_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
