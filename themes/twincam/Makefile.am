themedir = $(datadir)/plymouth/themes/twincam
nodist_theme_DATA = twincam.plymouth

MAINTAINERCLEANFILES = Makefile.in twincam.plymouth
CLEANFILES = twincam.plymouth

twincam.plymouth: $(srcdir)/twincam.plymouth.desktop
	$(AM_V_GEN) \
	sed -e 's,[@]PLYMOUTH_THEME_PATH[@],$(PLYMOUTH_THEME_PATH),g' \
	$(srcdir)/twincam.plymouth.desktop | \
	    $(MSGFMT) --desktop --template /proc/self/fd/0 -d $(top_srcdir)/po -o $@

EXTRA_DIST = twincam.plymouth.desktop
