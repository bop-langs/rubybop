
SHELL = /bin/sh

#### Start of system configuration section. ####
top_srcdir = $(topdir)/.
srcdir = $(top_srcdir)/ext/curses
topdir = ../..
hdrdir = $(top_srcdir)
VPATH = $(srcdir):$(topdir):$(hdrdir)
RUBYLIB = -
RUBYOPT = -rpurelib.rb
prefix = $(DESTDIR)/usr/local
exec_prefix = $(prefix)
vendordir = $(libdir)/ruby/vendor_ruby
psdir = $(docdir)
mandir = $(datarootdir)/man
sitearchdir = $(sitelibdir)/$(sitearch)
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
datarootdir = $(prefix)/share
archdir = $(rubylibdir)/$(arch)
pdfdir = $(docdir)
rubylibdir = $(libdir)/ruby/$(ruby_version)
libexecdir = $(exec_prefix)/libexec
docdir = $(datarootdir)/doc/$(PACKAGE)
infodir = $(datarootdir)/info
sbindir = $(exec_prefix)/sbin
vendorlibdir = $(vendordir)/$(ruby_version)
datadir = $(datarootdir)
dvidir = $(docdir)
sitelibdir = $(sitedir)/$(ruby_version)
sitedir = $(libdir)/ruby/site_ruby
localstatedir = $(prefix)/var
sharedstatedir = $(prefix)/com
htmldir = $(docdir)
oldincludedir = $(DESTDIR)/usr/include
includedir = $(prefix)/include
sysconfdir = $(prefix)/etc
localedir = $(datarootdir)/locale
vendorarchdir = $(vendorlibdir)/$(sitearch)

CC = gcc
LIBRUBY = $(LIBRUBY_A)
LIBRUBY_A = lib$(RUBY_SO_NAME)-static.a
LIBRUBYARG_SHARED = -Wl,-R -Wl,$(libdir) -L$(libdir) 
LIBRUBYARG_STATIC = -l$(RUBY_SO_NAME)-static

RUBY_EXTCONF_H = 
CFLAGS   =  -fPIC -g -O2 $(cflags) 
INCFLAGS = -I. -I../.. -I../../. -I../.././ext/curses
DEFS     = 
CPPFLAGS = -DHAVE_NCURSES_H -DHAVE_BEEP -DHAVE_BKGD -DHAVE_BKGDSET -DHAVE_CURS_SET -DHAVE_DELETELN -DHAVE_DOUPDATE -DHAVE_FLASH -DHAVE_GETBKGD -DHAVE_GETNSTR -DHAVE_ISENDWIN -DHAVE_KEYNAME -DHAVE_KEYPAD -DHAVE_RESIZETERM -DHAVE_SCRL -DHAVE_SETSCRREG -DHAVE_UNGETCH -DHAVE_WATTROFF -DHAVE_WATTRON -DHAVE_WATTRSET -DHAVE_WBKGD -DHAVE_WBKGDSET -DHAVE_WDELETELN -DHAVE_WGETNSTR -DHAVE_WRESIZE -DHAVE_WSCRL -DHAVE_WSETSCRREG -DHAVE_DEF_PROG_MODE -DHAVE_RESET_PROG_MODE -DHAVE_TIMEOUT -DHAVE_WTIMEOUT -DHAVE_NODELAY -DHAVE_INIT_COLOR -DHAVE_WCOLOR_SET -D_XOPEN_SOURCE_EXTENDED   
CXXFLAGS = $(CFLAGS) 
ldflags  = -L.  -rdynamic -Wl,-export-dynamic
dldflags = 
archflag = 
DLDFLAGS = $(ldflags) $(dldflags) $(archflag)
LDSHARED = $(CC) -shared
AR = ar
EXEEXT = 

RUBY_INSTALL_NAME = ruby
RUBY_SO_NAME = ruby
arch = x86_64-linux
sitearch = x86_64-linux
ruby_version = 1.8
ruby = $(topdir)/miniruby -I'$(topdir)' -I'$(top_srcdir)/lib' -I'$(extout)/$(arch)' -I'$(extout)/common' -I./- -I'$(top_srcdir)/ext' -rpurelib.rb
RUBY = $(ruby)
RM = rm -f
MAKEDIRS = mkdir -p
INSTALL = /bin/install -c
INSTALL_PROG = $(INSTALL) -m 0755
INSTALL_DATA = $(INSTALL) -m 644
COPY = cp

#### End of system configuration section. ####

preload = 

libpath = . $(topdir)
LIBPATH =  -L. -L$(topdir)
DEFFILE = 

CLEANFILES = mkmf.log
DISTCLEANFILES = 

extout = $(topdir)/.ext
extout_prefix = $(extout)$(target_prefix)/
target_prefix = 
LOCAL_LIBS = 
LIBS =  -lncurses -ltinfo  -lrt -ldl -lcrypt -lm   -lc
SRCS = curses.c
OBJS = curses.o
TARGET = curses
DLLIB = $(TARGET).so
EXTSTATIC = 
STATIC_LIB = $(TARGET).a

BINDIR        = $(extout)/bin
RUBYCOMMONDIR = $(extout)/common
RUBYLIBDIR    = $(RUBYCOMMONDIR)$(target_prefix)
RUBYARCHDIR   = $(extout)/$(arch)$(target_prefix)

TARGET_SO     = $(RUBYARCHDIR)/$(DLLIB)
CLEANLIBS     = $(RUBYARCHDIR)/$(TARGET).so $(RUBYARCHDIR)/$(TARGET).il? $(RUBYARCHDIR)/$(TARGET).tds $(RUBYARCHDIR)/$(TARGET).map
CLEANOBJS     = *.o *.a *.s[ol] *.pdb *.exp *.bak

all:		install
static:		$(STATIC_LIB) install-rb

clean:
		@-$(RM) $(CLEANLIBS) $(CLEANOBJS) $(CLEANFILES)

distclean:	clean
		@-$(RM) Makefile $(RUBY_EXTCONF_H) conftest.* mkmf.log
		@-$(RM) core ruby$(EXEEXT) *~ $(DISTCLEANFILES)

realclean:	distclean
install: install-so install-rb

install-so: $(RUBYARCHDIR)
install-so: $(RUBYARCHDIR)/$(DLLIB)
install-rb: pre-install-rb install-rb-default
install-rb-default: pre-install-rb-default
pre-install-rb: Makefile
pre-install-rb-default: Makefile
$(RUBYARCHDIR):
	$(MAKEDIRS) $@

site-install: site-install-so site-install-rb
site-install-so: install-so
site-install-rb: install-rb

.SUFFIXES: .c .m .cc .cxx .cpp .C .o

.cc.o:
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $<

.cxx.o:
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $<

.cpp.o:
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $<

.C.o:
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $<

.c.o:
	$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) -c $<

$(RUBYARCHDIR)/$(DLLIB): $(OBJS) Makefile
	@-$(RM) $@
	@-$(MAKEDIRS) $(@D)
	$(LDSHARED) -o $@ $(OBJS) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(STATIC_LIB): $(OBJS)
	$(AR) cru $@ $(OBJS)
	@-ranlib $(DLLIB) 2> /dev/null || true

###
curses.o: curses.c $(hdrdir)/ruby.h $(topdir)/config.h $(hdrdir)/defines.h
