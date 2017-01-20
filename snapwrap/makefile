.SUFFIXES : .dll .obj .c .h .def

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include

ifeq ($(INSTALL),)
INSTALL=ginstall
endif

CC = gcc
CFLAGS = -Zomf -Wall -O3

RM = rm -f

EXEPACK = lxlite /B- /L- /CS

ifeq ($(SNAP_BASE),)
SNAP_BASE=f:/lang/work/snap_sdk
endif

SNAP_INCLUDE = $(SNAP_BASE)/include
INC_PATH = -I.. -I$(SNAP_INCLUDE)

LD = gcc
LDFLAGS = -Zomf -Zdll

SNAP_LIB = $(SNAP_BASE)/lib/release/os232
LIB_PATH = -L$(SNAP_LIB)

.c.obj :
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

.PHONY : all

all : snapwrap.dll

snapwrap.dll : snapwrap.obj snapwrap.def
	$(LD) $(LDFLAGS) $(LIB_PATH) -o $@ $^ -lpm -ln_ga
	$(EXEPACK) $@

snapwrap.obj : snapwrap.c

clean :
	$(RM) *.bak
	$(RM) *.obj
	$(RM) *.dll

install : snapwrap.dll
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) snapwrap.dll $(DESTDIR)$(LIBDIR)

uninstall :
	$(RM) $(DESTDIR)$(LIBDIR)/snapwrap.dll

