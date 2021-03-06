CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

include $(top_srcdir)/Makefile.am.marshal

######################################
# Building the indicator
######################################

ayatanaappmenulibdir = $(INDICATORDIR)
ayatanaappmenulib_LTLIBRARIES = libayatana-appmenu.la
libayatana_appmenu_la_SOURCES = \
	dbus-shared.h \
	gdk-get-func.h \
	gdk-get-func.c \
	MwmUtil.h \
	indicator-appmenu.c \
	indicator-appmenu-marshal.c \
	window-menu.c \
	window-menu.h \
	window-menu-dbusmenu.c \
	window-menu-dbusmenu.h \
	window-menu-model.c \
	window-menu-model.h \
	gen-application-menu-renderer.xml.c \
	gen-application-menu-renderer.xml.h \
	gen-application-menu-registrar.xml.c \
	gen-application-menu-registrar.xml.h
libayatana_appmenu_la_CFLAGS = \
	$(INDICATOR_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-Wall -Wl,-Bsymbolic-functions -Wl,-z,defs -Wl,--as-needed -Werror -Wno-error=deprecated-declarations -DG_LOG_DOMAIN=\"Indicator-Appmenu\"
libayatana_appmenu_la_LIBADD = $(INDICATOR_LIBS) -lX11
libayatana_appmenu_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	-module -avoid-version

######################################
# Build your own marshaller
######################################

glib_marshal_list = indicator-appmenu-marshal.list
glib_marshal_prefix = _indicator_appmenu_marshal

######################################
# DBus Love
######################################

DBUS_SPECS = \
	application-menu-renderer.xml \
	application-menu-registrar.xml

gen-%.xml.c: %.xml
	@echo "Building $@ from $<"
	@echo "const char * _$(subst -,_,$(subst .,_,$(basename $(notdir $<)))) = " > $@
	@$(XSLT_PROC) $(srcdir)/clean-namespaces.xslt $< | \
		sed -e "s:\":\\\\\":g" -e s:^:\": -e s:\$$:\\\\n\": >> $@
	@echo ";" >> $@

gen-%.xml.h: %.xml
	@echo "Building $@ from $<"
	@echo "extern const char * _$(subst -,_,$(subst .,_,$(basename $(notdir $<))));" > $@

BUILT_SOURCES += \
	gen-application-menu-renderer.xml.c \
	gen-application-menu-renderer.xml.h \
	gen-application-menu-registrar.xml.c \
	gen-application-menu-registrar.xml.h

CLEANFILES += $(BUILT_SOURCES)

EXTRA_DIST += $(DBUS_SPECS) clean-namespaces.xslt
