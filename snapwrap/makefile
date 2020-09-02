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
CFLAGS = -Zomf -Wall -O3 -DOS2EMX_PLAIN_CHAR -funsigned-char

RM = rm -f

EXEPACK = lxlite /B- /L- /CS

BLDLEVEL_VENDOR := OS/2 Factory
BLDLEVEL_VERSION := 1.0.0
BLDLEVEL_DATE := $(shell LANG=C date +"\" %F %T %^Z  \"")
BLDLEVEL_HOST = $(shell echo $(HOSTNAME) | cut -b -11)
BLDLEVEL := @\#$(BLDLEVEL_VENDOR):$(BLDLEVEL_VERSION)\#@\#\#1\#\#$(BLDLEVEL_DATE)$(BLDLEVEL_HOST)::::::@@

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
	echo $(BLDLEVEL)Wrapper DLL for SNAP interface for K Video Accelerator >> $@

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

