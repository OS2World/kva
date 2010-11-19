.SUFFIXES : .dll .obj .c .h .def

ifeq ($(PREFIX),)
PREFIX=/usr
endif
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include

ifeq ($(INSTALL),)
INSTALL=ginstall
endif

CC = gcc
CFLAGS = -Zomf -Wall -O3

SNAP_INCLUDE = f:/lang/work/snap_sdk/include
RM = rm -f
INC_PATH = -I.. -I$(SNAP_INCLUDE)

LD = gcc
LDFLAGS = -Zomf -Zdll

SNAP_LIB = f:/lang/work/snap_sdk/lib/release/os232
LIB_PATH = -L$(SNAP_LIB)

.c.obj :
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

.PHONY : all

all : snapwrap.dll

snapwrap.dll : snapwrap.obj snapwrap.def
	$(LD) $(LDFLAGS) $(LIB_PATH) -o $@ $^ -lpm -ln_ga

snapwrap.obj : snapwrap.c

clean :
	$(RM) *.bak
	$(RM) *.obj
	$(RM) *.dll

install : snapwrap.dll
	$(INSTALL) -d $(LIBDIR)
	$(INSTALL) snapwrap.dll $(LIBDIR)

uninstall :
	$(RM) $(LIBDIR)/snapwrap.dll

