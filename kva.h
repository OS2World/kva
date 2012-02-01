/*
    K Video Accelerator library for OS/2
    Copyright (C) 2007-2012 by KO Myung-Hun <komh@chollian.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __KVA_H__
#define __KVA_H__

#include <os2.h>
#include <mmioos2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KVA_VERSION "1.1.3"

#define KVAM_AUTO   0
#define KVAM_DIVE   1
#define KVAM_WO     2
#define KVAM_SNAP   3
#define KVAM_VMAN   4

#define KVAR_NONE       0
#define KVAR_ORIGINAL   1
#define KVAR_FORCE43    2
#define KVAR_FORCE169   3
#define KVAR_FORCEANY   4

#define KVAE_NO_ERROR                      0
#define KVAE_NOT_INITIALIZED            ( -1 )
#define KVAE_ALREADY_INITIALIZED        ( -2 )
#define KVAE_INVALID_PARAMETER          ( -3 )
#define KVAE_ALREADY_LOCKED             ( -4 )
#define KVAE_NOT_LOCKED                 ( -5 )
#define KVAE_CANNOT_SUBCLASS            ( -6 )
#define KVAE_CANNOT_LOAD_WO             ( -7 )
#define KVAE_NO_SUPPORTED_FOURCC        ( -8 )
#define KVAE_WO_PALETTIZED_MODE         ( -9 )
#define KVAE_NO_ATTRIBUTE               ( -10 )
#define KVAE_CANNOT_LOAD_SNAP_WRAPPER   ( -11 )
#define KVAE_CANNOT_LOAD_SNAP_DRIVER    ( -12 )
#define KVAE_CANNOT_ALLOC_VIDEO_BUFFER  ( -13 )
#define KVAE_CANNOT_SETUP               ( -14 )
#define KVAE_HW_IN_USE                  ( -15 )
#define KVAE_NOT_ENOUGH_MEMORY          ( -16 )
#define KVAE_CANNOT_LOAD_DIVE           ( -17 )
#define KVAE_CANNOT_LOAD_VMAN           ( -18 )
#define KVAE_VMAN_ERROR                 ( -19 )

#define KVAA_BRIGHTNESS 0
#define KVAA_CONTRAST   1
#define KVAA_SATURATION 2
#define KVAA_HUE        3
#define KVAA_GAMMA      4
#define KVAA_LAST       ( KVAA_GAMMA + 1 )

#define KVAF_YUY2       0x00000001
#define KVAF_YV12       0x00000002
#define KVAF_YVU9       0x00000004
#define KVAF_BGR24      0x00010000
#define KVAF_BGR16      0x00020000
#define KVAF_BGR15      0x00040000
#define KVAF_BGR32      0x00080000

#ifndef FOURCC_YV12
#define FOURCC_YV12     mmioFOURCC( 'Y', 'V', '1', '2' )
#endif

#ifndef FOURCC_YVU9
#define FOURCC_YVU9     mmioFOURCC( 'Y', 'V', 'U', '9' )
#endif

#pragma pack( 1 )

typedef struct tagKVASETUP
{
    ULONG       ulLength;         //size of structure in bytes
    RECTL       rclSrcRect;       //displayed subrectangle, top-left is (0,0)
    SIZEL       szlSrcSize;       //source image size
    ULONG       ulRatio;          //aspect ratio
    ULONG       ulAspectWidth;    //aspect width, only for KVAR_FORCEANY
    ULONG       ulAspectHeight;   //aspect height, only for KVAR_FORCEANY
    ULONG       fccSrcColor;      //image format
    BOOL        fInvert;          //image invert, only for DIVE
    BOOL        fDither;          //image dither, only for DIVE
} KVASETUP, *PKVASETUP;

typedef struct tagKVACAPS
{
    ULONG   ulMode;
    ULONG   ulDepth;
    ULONG   cxScreen;
    ULONG   cyScreen;
    FOURCC  fccScreen;
    ULONG   ulInputFormatFlags;
    ULONG   ulRMask;
    ULONG   ulGMask;
    ULONG   ulBMask;
} KVACAPS, *PKVACAPS;

#pragma pack()

APIRET APIENTRY kvaInit( ULONG kvaMode, HWND hwnd, ULONG ulKeyColor );
APIRET APIENTRY kvaDone( VOID );
APIRET APIENTRY kvaLockBuffer( PPVOID ppBuffer, PULONG pulBPL );
APIRET APIENTRY kvaUnlockBuffer( VOID );
APIRET APIENTRY kvaSetup( PKVASETUP pkvas );
APIRET APIENTRY kvaCaps( PKVACAPS pkvac );
APIRET APIENTRY kvaClearRect( PRECTL prcl );
APIRET APIENTRY kvaAdjustDstRect( PRECTL prclSrc, PRECTL prclDst );
APIRET APIENTRY kvaQueryAttr( ULONG ulAttr, PULONG pulValue );
APIRET APIENTRY kvaSetAttr( ULONG ulAttr, PULONG pulValue );
APIRET APIENTRY kvaResetAttr( VOID );
APIRET APIENTRY kvaDisableScreenSaver( VOID );
APIRET APIENTRY kvaEnableScreenSaver( VOID );

#ifdef __cplusplus
}
#endif

#endif
