# Makefile for kLIBC/GNU Make
.PHONY : all

.SUFFIXES : .exe .a .lib .o .c .h

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

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

.a.lib :
	emxomf -o $@ $<

all : kva.a kva.lib kvademo.exe
	$(MAKE) -C snapwrap all

kva.a : kva.o kva_dive.o kva_wo.o kva_snap.o
	$(AR) rc $@ $^

kva.o : kva.c kva.h kva_internal.h kva_dive.h kva_wo.h kva_snap.h

kva_dive.o : kva_dive.c kva.h kva_internal.h kva_dive.h

kva_wo.o : kva_wo.c hwvideo.h kva.h kva_internal.h kva_wo.h

kva_snap.o : kva_snap.c kva.h kva_internal.h kva_snap.h

kvademo.exe : kvademo.o kva.a kvademo.def
	$(CC) $(LDFLAGS) -o $@ $^

kvademo.o : kvademo.c kva.h mpeg.h

clean :
	-$(RM) *.bak
	-$(RM) *.o
	-$(RM) *.a
	-$(RM) *.lib
	-$(RM) *.exe
	$(MAKE) -C snapwrap clean

dist : src
	mkdir kva_dist
	$(MAKE) install PREFIX=$(shell pwd)/kva_dist
	( cd kva_dist && zip -rpSm ../libkva$(VER).zip * )
	rmdir kva_dist
	zip -m libkva$(VER).zip src.zip

distclean : clean
	-$(RM) *.zip

src : kva.c kva_dive.c kva_snap.c kva_wo.c \
      kva.h kva_internal.h kva_dive.h kva_snap.h kva_wo.h hwvideo.h \
      Makefile \
      kvademo.c kvademo.def mpeg.h mpegdec.dll demo.mpg \
      snapwrap/snapwrap.c snapwrap/snapwrap.def snapwrap/makefile
	-$(RM) src.zip
	zip src.zip $^

install : kva.a kva.lib kva.h
	$(INSTALL) -d $(LIBDIR)
	$(INSTALL) -d $(INCDIR)
	$(INSTALL) kva.a $(LIBDIR)
	$(INSTALL) kva.lib $(LIBDIR)
	$(INSTALL) kva.h $(INCDIR)
	$(MAKE) -C snapwrap install PREFIX=$(PREFIX) INSTALL=$(INSTALL)

uninstall :
	-$(RM) $(LIBDIR)/kva.a $(LIBDIR)/kva.lib
	-$(RM) $(INCDIR)/kva.h
	$(MAKE) -C snapwrap uninstall


