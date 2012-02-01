# Makefile for OpenWatcom/WMAKE
.ERASE

.SUFFIXES :
.SUFFIXES : .exe .lib .obj .c .h

CC = wcc386
CFLAGS = -zq -wx -bm -d0 -oaxt

LINK = wlink
LFLAGS = option quiet

RM = del

.c.obj :
    $(CC) $(CFLAGS) -fo=$@ $[@

all : .SYMBOLIC kva.lib kvademo.exe

kva.lib : kva.obj kva_dive.obj kva_wo.obj kva_snap.obj kva_vman.obj
    -$(RM) $@
    wlib -b $@ $<

kva.obj : kva.c kva.h kva_internal.h kva_dive.h kva_wo.h kva_snap.h

kva_dive.obj : kva_dive.c kva.h kva_internal.h kva_dive.h

kva_wo.obj : kva_wo.c hwvideo.h kva.h kva_internal.h kva_wo.h

kva_snap.obj : kva_snap.c kva.h kva_internal.h kva_snap.h

kva_vman.obj : kva_vman.c kva.h kva_internal.h kva_vman.h

kvademo.exe : kvademo.obj kva.lib
    $(LINK) $(LFLAGS) system os2v2_pm name $@ file { $< }

kvademo.obj : kvademo.c kva.h mpeg.h

clean : .SYMBOLIC
    -$(RM) *.bak
    -$(RM) *.obj
    -$(RM) *.lib
    -$(RM) *.exe

