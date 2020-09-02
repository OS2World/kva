# Makefile for kLIBC/GNU Make
.PHONY : all

.SUFFIXES : .exe .a .lib .dll .o .c .h .def

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include

ifeq ($(INSTALL),)
INSTALL=ginstall
endif

CC = gcc
CFLAGS = -Wall -O3 -DOS2EMX_PLAIN_CHAR -funsigned-char
LDFLAGS =

AR = ar

RM = rm -f

BLDLEVEL_VENDOR := OS/2 Factory
BLDLEVEL_VERSION_MACRO := KVA_VERSION
BLDLEVEL_VERSION_FILE := kva.h
BLDLEVEL_VERSION := $(shell sed -n -e "s/^[ \t]*\#[ \t]*define[ \t]\+$(BLDLEVEL_VERSION_MACRO)[ \t]\+\"\(.*\)\"/\1/p" $(BLDLEVEL_VERSION_FILE))
BLDLEVEL_DATE := $(shell LANG=C date +"\" %F %T %^Z  \"")
BLDLEVEL_HOST = $(shell echo $(HOSTNAME) | cut -b -11)
BLDLEVEL := @\#$(BLDLEVEL_VENDOR):$(BLDLEVEL_VERSION)\#@\#\#1\#\#$(BLDLEVEL_DATE)$(BLDLEVEL_HOST)::::::@@

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
	echo $(BLDLEVEL)K Video Accelerator >> $@

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
	echo $(BLDLEVEL)KVA demo >> $@

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

distclean : clean
	$(RM) libkva-*

src : kva.c kva_dive.c kva_snap.c kva_wo.c \
      kva.h kva_internal.h kva_dive.h kva_snap.h kva_wo.h hwvideo.h \
      kva_vman.c kva_vman.h gradd.h \
      Makefile Makefile.icc Makefile.wat \
      kvademo.c kvademo.def mpeg.h mpegdec.dll demo.mpg \
      snapwrap/snapwrap.c snapwrap/snapwrap.def snapwrap/makefile
	$(RM) libkva-$(VER)-src.zip
	$(RM) -r libkva-$(VER)
	zip libkva-$(VER)-src.zip $^
	unzip libkva-$(VER)-src.zip -d libkva-$(VER)
	$(RM) -r libkva-$(VER)-src.zip
	zip -rpSm libkva-$(VER)-src.zip libkva-$(VER)

install : kva.a kva.lib kva_dll.a kva_dll.lib $(KVADLL) kva.h
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)$(INCDIR)
	$(INSTALL) kva.a $(DESTDIR)$(LIBDIR)
	$(INSTALL) kva.lib $(DESTDIR)$(LIBDIR)
	$(INSTALL) kva_dll.a $(DESTDIR)$(LIBDIR)
	$(INSTALL) kva_dll.lib $(DESTDIR)$(LIBDIR)
	$(INSTALL) $(KVADLL) $(DESTDIR)$(LIBDIR)
	$(INSTALL) kva.h $(DESTDIR)$(INCDIR)
	$(MAKE) -C snapwrap install PREFIX=$(PREFIX) INSTALL=$(INSTALL) DESTDIR=$(DESTDIR)

uninstall :
	$(RM) $(DESTDIR)$(LIBDIR)/kva.a $(DESTDIR)$(LIBDIR)/kva.lib
	$(RM) $(DESTDIR)$(LIBDIR)/kva_dll.a $(DESTDIR)$(LIBDIR)/kva_dll.lib
	$(RM) $(DESTDIR)$(LIBDIR)/$(KVADLL)
	$(RM) $(DESTDIR)$(INCDIR)/kva.h
	$(MAKE) -C snapwrap uninstall DESTDIR=$(DESTDIR)
