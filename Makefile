include Makefile.sets

# CMake is the preferred build. Keep the historical make path working too.
# config.h is selected via this include path in the legacy build.
CPPFLAGS += -I.

# Building for Windows (opt/xmingw is for Gentoo):
#HOST=/usr/local/mingw32/bin/i586-mingw32msvc-
#HOST=/opt/xmingw/bin/i386-mingw32msvc-
#CFLAGS +=
#CPPFLAGS +=
#CXXFLAGS +=
##LDOPTS = -L/usr/local/mingw32/lib
#LDOPTS += -L/opt/xmingw/lib
#LDFLAGS +=

# Building for native:
#HOST=
LDFLAGS +=-static # for statically building

#CXX=$(HOST)g++
#CC=$(HOST)gcc
#CPP=$(HOST)gcc

CXX += -flto
#CXX += -pg -flto
#OPTIM= -O2 -finline
OPTIM= -O3 -ffast-math

#OPTIM=-Og -fno-inline
#CXXFLAGS += -fsanitize=address

VERSION = 3.39.1

BINDIR=/usr/local/bin
INSTALL=install

PROG=dirr
OBJS=main.o pwfun.o cons.o setfun.o strfun.o colouring.o \
     getname.o getsize.o totals.o argh.o \
     dfa_match.o printf.o

ARCHDIR=archives/
ARCHNAME=dirr-$(VERSION)
ARCHFILES=main.cc COPYING ChangeLog README README.md dirrsets.hh config.h config.h.in \
          configure config.sub1 config.sub2 config.sub3 \
          CMakeLists.txt .gitignore tests/cli-smoke.sh \
          dfa_match.cc dfa_match.hh \
          colouring.cc colouring.hh \
          getname.cc getname.hh \
          getsize.cc getsize.hh \
          setfun.cc setfun.hh \
          strfun.cc strfun.hh \
          totals.cc totals.hh \
          pwfun.cc pwfun.hh \
          cons.cc cons.hh \
          argh.cc argh.hh \
          printf.cc printf.hh \
          stat.h \
          TODO progdesc.php \
          Makefile.sets.in \
          Makefile.sets \
          workaround/string_view

INSTALLPROGS=$(PROG)

$(PROG): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

argh.o: argh.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DColourPrints -o $@ -c $<

clean:
	rm -f $(PROG) $(OBJS) .depend .depend.* configure.log

distclean: clean
	rm -f Makefile.cfg *~
realclean: distclean

include depfun.mak
