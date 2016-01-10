# Makefile for kLIBC/GNU Make
.PHONY : all

.SUFFIXES : .exe .a .lib .dll .o .c .h .def

ifeq ($(PREFIX),)
PREFIX=/usr
endif
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include

ifeq ($(INSTALL),)
INSTALL=ginstall
endif

CC = gcc
CFLAGS = -Wall -O3
LDFLAGS =

AR = ar

RM = rm -f

KVAOBJS = kva.o kva_dive.o kva_wo.o kva_snap.o kva_vman.o

DLLVER = 0
KVADLL = kva$(DLLVER).dll
KVADLLNAME = kva$(DLLVER)
KVADLLDEF = kvadll.def

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

.a.lib :
	emxomf -o $@ $<

all : kva.a kva.lib kva_dll.a kva_dll.lib $(KVADLL) kvademo.exe
	$(MAKE) -C snapwrap all

kva.a : $(KVAOBJS)
	$(AR) rc $@ $^

kva_dll.a : $(KVADLL)
	emximp -o $@ $(KVADLL)

$(KVADLL) : $(KVAOBJS) $(KVADLLDEF)
	$(CC) -Zdll $(LDFLAGS) -o $@ $^

$(KVADLLDEF) :
	echo LIBRARY $(KVADLLNAME) INITINSTANCE TERMINSTANCE > $@
	echo DATA MULTIPLE NONSHARED >> $@
	echo EXPORTS >> $@
	echo     kvaInit >> $@
	echo     kvaAdjustDstRect >> $@
	echo     kvaDone >> $@
	echo     kvaLockBuffer >> $@
	echo     kvaUnlockBuffer >> $@
	echo     kvaSetup >> $@
	echo     kvaCaps >> $@
	echo     kvaClearRect >> $@
	echo     kvaQueryAttr >> $@
	echo     kvaSetAttr >> $@
	echo     kvaResetAttr >> $@
	echo     kvaDisableScreenSaver >> $@
	echo     kvaEnableScreenSaver >> $@

kva.o : kva.c kva.h kva_internal.h kva_dive.h kva_wo.h kva_snap.h kva_vman.h

kva_dive.o : kva_dive.c kva.h kva_internal.h kva_dive.h

kva_wo.o : kva_wo.c hwvideo.h kva.h kva_internal.h kva_wo.h

kva_snap.o : kva_snap.c kva.h kva_internal.h kva_snap.h

kva_vman.o : kva_vman.c kva.h kva_internal.h kva_vman.h

kvademo.exe : kvademo.o kva.a kvademo.def
	$(CC) $(LDFLAGS) -o $@ $^

kvademo.o : kvademo.c kva.h mpeg.h

clean :
	$(RM) *.bak
	$(RM) *.o
	$(RM) *.a
	$(RM) *.lib
	$(RM) $(KVADLL)
	$(RM) $(KVALLDEF)
	$(RM) *.exe
	$(MAKE) -C snapwrap clean

dist : src
	mkdir kva_dist
	$(MAKE) install PREFIX=$(shell pwd)/kva_dist
	( cd kva_dist && zip -rpSm ../libkva-$(VER).zip * )
	rmdir kva_dist
	zip -m libkva-$(VER).zip src.zip

distclean : clean
	$(RM) *.zip

src : kva.c kva_dive.c kva_snap.c kva_wo.c \
      kva.h kva_internal.h kva_dive.h kva_snap.h kva_wo.h hwvideo.h \
      kva_vman.c kva_vman.h gradd.h \
      Makefile Makefile.icc Makefile.wat \
      kvademo.c kvademo.def mpeg.h mpegdec.dll demo.mpg \
      snapwrap/snapwrap.c snapwrap/snapwrap.def snapwrap/makefile
	$(RM) src.zip
	zip src.zip $^

install : kva.a kva.lib kva_dll.a kva_dll.lib $(KVADLL) kva.h
	$(INSTALL) -d $(LIBDIR)
	$(INSTALL) -d $(INCDIR)
	$(INSTALL) kva.a $(LIBDIR)
	$(INSTALL) kva.lib $(LIBDIR)
	$(INSTALL) kva_dll.a $(LIBDIR)
	$(INSTALL) kva_dll.lib $(LIBDIR)
	$(INSTALL) $(KVADLL) $(LIBDIR)
	$(INSTALL) kva.h $(INCDIR)
	$(MAKE) -C snapwrap install PREFIX=$(PREFIX) INSTALL=$(INSTALL)

uninstall :
	$(RM) $(LIBDIR)/kva.a $(LIBDIR)/kva.lib
	$(RM) $(LIBDIR)/kva_dll.a $(LIBDIR)/kva_dll.lib
	$(RM) $(LIBDIR)/$(KVADLL)
	$(RM) $(INCDIR)/kva.h
	$(MAKE) -C snapwrap uninstall
