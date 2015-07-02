/*
    Wrapper DLL for SNAP interface for K Video Accelerator
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

#define INCL_DOS
#include <os2.h>

#include <mmioos2.h>
#include <fourcc.h>

#include "snap/gasdk.h"

#include "kva.h"

#if 0
// the following vars are declared in gasdk.c/n_ga.lib
GA_devCtx       *dc;
GA_modeInfo      modeInfo;
GA_bufferFuncs   bufmgr;
REF2D_driver    *ref2d;
#endif

// the following var is required for gasdk.c
GA_driverFuncs driver;

static GA_initFuncs     m_initFuncs = { 0, };
static GA_videoInf     *m_videoInf = NULL;
static GA_videoFuncs    m_videoFuncs = { 0, };

static LONG m_lBrightness;
static LONG m_lContrast;
static LONG m_lSaturation;
static LONG m_lHue;
static LONG m_lGamma = 0;

APIRET APIENTRY swLoadDriver( VOID );
APIRET APIENTRY swUnloadDriver( VOID );
APIRET APIENTRY swSetVideoOutput( PVOID pVideoBuf, LONG srcX, LONG srcY, LONG srcCX, LONG srcCY, LONG dstX, LONG dstY, LONG dstCX, LONG dstCY, FOURCC fcc );
APIRET APIENTRY swMoveVideoOutput(LONG srcX, LONG srcY, LONG srcCX, LONG srcCY, LONG dstX, LONG dstY, LONG dstCX, LONG dstCY );
APIRET APIENTRY swDisableVideoOutput( VOID );
APIRET APIENTRY swSetDstVideoColorKey( ULONG ulKeyColor );
APIRET APIENTRY swAllocVideoBuffers( PPVOID ppVideoBuf, LONG cx, LONG cy, FOURCC fcc, LONG nBufs );
APIRET APIENTRY swFreeVideoBuffers( PVOID pVideoBuf );
APIRET APIENTRY swLockBuffer( PVOID pVideoBuf, PPVOID ppSurface, PULONG pulBPL );
APIRET APIENTRY swUnlockBuffer( PVOID pVideoBuf );
APIRET APIENTRY swCaps( PKVACAPS pkvac );
APIRET APIENTRY swQueryVideoBrightness( PULONG pulValue );
APIRET APIENTRY swSetVideoBrightness( PULONG pulValue );
APIRET APIENTRY swQueryVideoContrast( PULONG pulValue );
APIRET APIENTRY swSetVideoContrast( PULONG pulValue );
APIRET APIENTRY swQueryVideoSaturation( PULONG pulValue );
APIRET APIENTRY swSetVideoSaturation( PULONG pulValue );
APIRET APIENTRY swQueryVideoHue( PULONG pulValue );
APIRET APIENTRY swSetVideoHue( PULONG pulValue );
APIRET APIENTRY swQueryVideoGammaCorrect( PULONG pulValue );
APIRET APIENTRY swSetVideoGammaCorrect( PULONG pulValue );

#define SNAP_YUY2       ( gaVideoYUV422 | gaVideoYUYV )
#define SNAP_YV12       ( gaVideoYUV12 | gaVideoYVU )
#define SNAP_YVU9       ( gaVideoYUV9 | gaVideoYVU )

static inline ULONG fcc2snap( FOURCC fcc )
{
    switch( fcc )
    {
        case FOURCC_Y422 :
            return SNAP_YUY2;

        case FOURCC_YV12 :
            return SNAP_YV12;

        case FOURCC_YVU9 :
            return SNAP_YVU9;
    }

    return 0;
}

static int isHelperAvailable( VOID )
{
    HFILE hSDDHelp;
    ULONG ulAction;

    HMODULE hmod;

    if( DosOpen( "SDDHELP$", &hSDDHelp, &ulAction, 0, 0,
                 FILE_OPEN, OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
                 NULL ))
        return 0;

    DosClose( hSDDHelp );

    if (DosQueryModuleHandle("sddgradd", &hmod))
        return 0;

    return 1;
}

APIRET APIENTRY swLoadDriver( VOID )
{
    dc = NULL;
    memset( &m_initFuncs, 0, sizeof( m_initFuncs ));
    memset( &modeInfo, 0, sizeof( modeInfo ));
    m_videoInf = NULL;
    memset( &m_videoFuncs, 0, sizeof( m_videoFuncs ));
    memset( &bufmgr, 0, sizeof( bufmgr ));
    ref2d = NULL;

    if( !isHelperAvailable())
        return -1;

    dc = GA_loadDriver( 0, false );
    if( !dc )
        return -1;

    m_initFuncs.dwSize = sizeof( m_initFuncs );
    if( !GA_queryFunctions( dc, GA_GET_INITFUNCS, &m_initFuncs ))
        goto exit_error;

    driver.dwSize = sizeof( driver );
    if( !GA_queryFunctions( dc, GA_GET_DRIVERFUNCS, &driver ))
        goto exit_error;

    modeInfo.dwSize = sizeof( modeInfo );
    m_initFuncs.GetCurrentVideoModeInfo( &modeInfo );
    if( !modeInfo.VideoWindows )
        goto exit_error;

    ref2d = GA_getCurrentRef2d( 0 );
    if( !ref2d )
        goto exit_error;

    m_videoFuncs.dwSize = sizeof( m_videoFuncs );
    if( !REF2D_queryFunctions( ref2d, GA_GET_VIDEOFUNCS, &m_videoFuncs ))
        goto exit_error;

    bufmgr.dwSize = sizeof( bufmgr );
    if( !REF2D_queryFunctions( ref2d, GA_GET_BUFFERFUNCS, &bufmgr ))
        goto exit_error;

    m_videoInf = modeInfo.VideoWindows[ 0 ];

    m_lBrightness = m_videoInf->VideoBrightnessDefault;
    m_lContrast = m_videoInf->VideoContrastDefault;
    m_lSaturation = m_videoInf->VideoSaturationDefault;
    m_lHue = m_videoInf->VideoHueDefault;
    m_lGamma = 0;

    return 0;

exit_error :
    GA_unloadDriver( dc );

    return -1;
}

APIRET APIENTRY swUnloadDriver( VOID )
{
    m_videoFuncs.DisableVideoOutput( 0 );

    GA_unloadDriver( dc );

    return 0;
}

APIRET APIENTRY swSetVideoOutput( PVOID pVideoBuf,
                                  LONG srcX, LONG srcY, LONG srcCX, LONG srcCY,
                                  LONG dstX, LONG dstY, LONG dstCX, LONG dstCY,
                                  FOURCC fcc )
{
    ULONG ulVOFlags;

    ulVOFlags = fcc2snap( fcc );

    if( m_videoInf->VideoOutputFlags & gaVideoColorKeyDstSingle )
        ulVOFlags |= gaVideoColorKeyDstSingle;

    if( m_videoInf->VideoOutputFlags & gaVideoXInterp )
        ulVOFlags |= gaVideoXInterp;

    if( m_videoInf->VideoOutputFlags & gaVideoYInterp )
        ulVOFlags |= gaVideoYInterp;

    return !m_videoFuncs.SetVideoOutput( 0, 0, pVideoBuf,
                                         srcX, srcY, srcCX, srcCY,
                                         dstX, modeInfo.YResolution - dstY, dstCX, dstCY,
                                         ulVOFlags );
}

APIRET APIENTRY swMoveVideoOutput( LONG srcX, LONG srcY, LONG srcCX, LONG srcCY,
                                   LONG dstX, LONG dstY, LONG dstCX, LONG dstCY )
{
    m_videoFuncs.MoveVideoOutput( 0,
                                  srcX, srcY, srcCX, srcCY,
                                  dstX, modeInfo.YResolution - dstY, dstCX, dstCY,
                                  0 );

    return 0;
}

APIRET APIENTRY swDisableVideoOutput( VOID )
{
    m_videoFuncs.DisableVideoOutput( 0 );

    return 0;
}

APIRET APIENTRY swSetDstVideoColorKey( ULONG ulKeyColor )
{
    ULONG ulRGB;

    // convert to screen color format
    ulRGB = rgbColorEx(( BYTE )( ulKeyColor >> 16 ),
                       ( BYTE )( ulKeyColor >> 8 ),
                       ( BYTE )  ulKeyColor,
                       &modeInfo.PixelFormat );

    m_videoFuncs.SetDstVideoColorKey( 0, ulRGB, ulRGB );

    return 0;
}

APIRET APIENTRY swAllocVideoBuffers( PPVOID ppVideoBuf, LONG cx, LONG cy, FOURCC fcc, LONG nBufs )
{
    *ppVideoBuf = m_videoFuncs.AllocVideoBuffers( cx, cy, fcc2snap( fcc ), nBufs );
    if( !*ppVideoBuf )
        return -1;

    return 0;
}

APIRET APIENTRY swFreeVideoBuffers( PVOID pVideoBuf )
{
    m_videoFuncs.FreeVideoBuffers( pVideoBuf );

    return 0;
}

APIRET APIENTRY swLockBuffer( PVOID pVideoBuf, PPVOID ppBuffer, PULONG pulBPL )
{
    bufmgr.LockBuffer( pVideoBuf );

    *ppBuffer = (( GA_buf * )pVideoBuf )->Surface;
    *pulBPL = (( GA_buf * )pVideoBuf )->Stride;

    return 0;
}

APIRET APIENTRY swUnlockBuffer( PVOID pVideoBuf )
{
    bufmgr.UnlockBuffer( pVideoBuf );

    return 0;
}

APIRET APIENTRY swCaps( PKVACAPS pkvac )
{
    pkvac->ulMode = KVAM_SNAP;
    pkvac->ulDepth = modeInfo.BitsPerPixel;
    pkvac->cxScreen = modeInfo.XResolution;
    pkvac->cyScreen = modeInfo.YResolution;

    switch( modeInfo.BitsPerPixel )
    {
        case 32 :
            pkvac->fccScreen = FOURCC_BGR4;
            break;

        case 24 :
            pkvac->fccScreen = FOURCC_BGR3;
            break;

        case 16 :
            pkvac->fccScreen = FOURCC_R565;
            break;

        case 15 :
            pkvac->fccScreen = FOURCC_R555;
            break;

        case  8 :
        default :   // right ?
            pkvac->fccScreen = FOURCC_LUT8;
            break;
    }

    pkvac->ulInputFormatFlags = 0;
    if(( m_videoInf->VideoInputFormats & SNAP_YUY2 ) == SNAP_YUY2 )
        pkvac->ulInputFormatFlags |= KVAF_YUY2;

    if(( m_videoInf->VideoInputFormats & SNAP_YV12 ) == SNAP_YV12 )
        pkvac->ulInputFormatFlags |= KVAF_YV12;

    if(( m_videoInf->VideoInputFormats & SNAP_YVU9 ) == SNAP_YVU9 )
        pkvac->ulInputFormatFlags |= KVAF_YVU9;

    return 0;
}

#define ATTR_FROM_SNAP( value, min, max )   ((( value ) - ( min )) * 255 / (( max ) - ( min )))
#define ATTR_TO_SNAP( value, min, max )     ((( value ) + 1 ) * (( max ) - ( min )) / 255 + ( min ))

APIRET APIENTRY swQueryVideoBrightness( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoBrightness )
        return -1;

    *pulValue = ATTR_FROM_SNAP( m_lBrightness, m_videoInf->VideoBrightnessMin, m_videoInf->VideoBrightnessMax );

    return 0;
}

APIRET APIENTRY swSetVideoBrightness( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoBrightness )
        return -1;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = ATTR_FROM_SNAP( m_videoInf->VideoBrightnessDefault, m_videoInf->VideoBrightnessMin, m_videoInf->VideoBrightnessMax );

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    m_lBrightness = ATTR_TO_SNAP( *pulValue, m_videoInf->VideoBrightnessMin, m_videoInf->VideoBrightnessMax );

    m_videoFuncs.SetVideoBrightness( 0, m_lBrightness );

    return 0;
}

APIRET APIENTRY swQueryVideoContrast( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoContrast )
        return -1;

    *pulValue = ATTR_FROM_SNAP( m_lContrast, m_videoInf->VideoContrastMin, m_videoInf->VideoContrastMax );

    return 0;
}

APIRET APIENTRY swSetVideoContrast( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoContrast )
        return -1;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = ATTR_FROM_SNAP( m_videoInf->VideoContrastDefault, m_videoInf->VideoContrastMin, m_videoInf->VideoContrastMax );

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    m_lContrast = ATTR_TO_SNAP( *pulValue, m_videoInf->VideoContrastMin, m_videoInf->VideoContrastMax );

    m_videoFuncs.SetVideoContrast( 0, m_lContrast );

    return 0;
}

APIRET APIENTRY swQueryVideoSaturation( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoSaturation )
        return -1;

    *pulValue = ATTR_FROM_SNAP( m_lSaturation, m_videoInf->VideoSaturationMin, m_videoInf->VideoSaturationMax );

    return 0;
}

APIRET APIENTRY swSetVideoSaturation( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoSaturation )
        return -1;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = ATTR_FROM_SNAP( m_videoInf->VideoSaturationDefault, m_videoInf->VideoSaturationMin, m_videoInf->VideoSaturationMax );

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    m_lSaturation = ATTR_TO_SNAP( *pulValue, m_videoInf->VideoSaturationMin, m_videoInf->VideoSaturationMax );

    m_videoFuncs.SetVideoSaturation( 0, m_lSaturation );

    return 0;
}

APIRET APIENTRY swQueryVideoHue( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoHue )
        return -1;

    *pulValue = ATTR_FROM_SNAP( m_lHue, m_videoInf->VideoHueMin, m_videoInf->VideoHueMax );

    return 0;
}

APIRET APIENTRY swSetVideoHue( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoHue )
        return -1;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = ATTR_FROM_SNAP( m_videoInf->VideoHueDefault, m_videoInf->VideoHueMin, m_videoInf->VideoHueMax );

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    m_lHue = ATTR_TO_SNAP( *pulValue, m_videoInf->VideoHueMin, m_videoInf->VideoHueMax );

    m_videoFuncs.SetVideoHue( 0, m_lHue );

    return 0;
}

/* Gamma range : 0.01 - 2.55, default = 0 */

APIRET APIENTRY swQueryVideoGammaCorrect( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoGammaCorrect )
        return -1;

    *pulValue = m_lGamma;

    return 0;
}

APIRET APIENTRY swSetVideoGammaCorrect( PULONG pulValue )
{
    if( !m_videoFuncs.SetVideoGammaCorrect )
        return -1;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = 0;

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    m_lGamma = *pulValue;

    m_videoFuncs.SetVideoGammaCorrect( 0, m_lGamma );

    return 0;
}


