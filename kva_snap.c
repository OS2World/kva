/*
    SNAP interface for K Video Accelerator
    Copyright (C) 2007 by KO Myung-Hun <komh@chollian.net>

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

    Changes :
        KO Myung-Hun <komh@chollian.net> 2008/01/07
            - SetVideoOutput() reset all attributes, so use it only one at
              setup. And do not call DisableVideoOutput() when destination
              rect is empty. Use workaround.

        KO Myung-Hun <komh@chollian.net> 2008/01/17
            - Use fccSrcColor variable to support YV12, YVU9 color format
*/

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <mmioos2.h>
#include <fourcc.h>

#include <stdlib.h>
#include <string.h>

#include "kva.h"
#include "kva_internal.h"
#include "kva_snap.h"

typedef struct _SNAPSETUP
{
    LONG    lSrcX;
    LONG    lSrcY;
    LONG    lSrcCX;
    LONG    lSrcCY;
} SNAPSETUP, *PSNAPSETUP;

static SNAPSETUP    m_ss = { 0, };

static HMODULE  m_swHandle = NULLHANDLE;

static PVOID    m_pVideoBuf = NULL;

static PFNWP    m_pfnwpOld = NULL;

static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWLoadDriver, ( VOID ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWUnloadDriver, ( VOID ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoOutput, ( PVOID, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, FOURCC ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWMoveVideoOutput, ( LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWDisableVideoOutput, ( VOID ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetDstVideoColorKey, ( ULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWAllocVideoBuffers, ( PPVOID, LONG, LONG, FOURCC, LONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWFreeVideoBuffers, ( PVOID ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWLockBuffer, ( PVOID, PPVOID, PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWUnlockBuffer, ( PVOID ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWCaps, ( PKVACAPS ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWQueryVideoBrightness, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoBrightness, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWQueryVideoContrast, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoContrast, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWQueryVideoSaturation, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoSaturation, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWQueryVideoHue, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoHue, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWQueryVideoGammaCorrect, ( PULONG ));
static DECLARE_PFN( APIRET, APIENTRY, m_pfnSWSetVideoGammaCorrect, ( PULONG ));

static APIRET APIENTRY snapDone( VOID );
static APIRET APIENTRY snapLockBuffer( PPVOID ppBuffer, PULONG pulBPL );
static APIRET APIENTRY snapUnlockBuffer( VOID );
static APIRET APIENTRY snapSetup( PKVASETUP pkvas );
static APIRET APIENTRY snapCaps( PKVACAPS pkvac );
static APIRET APIENTRY snapQueryAttr( ULONG ulAttr, PULONG pulValue );
static APIRET APIENTRY snapSetAttr( ULONG ulAttr, PULONG pulValue );

static BOOL LoadWrapper( VOID )
{
    CHAR szTempStr[ 255 ];

    // Load Wrapper DLL
    if( DosLoadModule( szTempStr, sizeof( szTempStr ), "snapwrap.dll", &m_swHandle ))
        return FALSE;

    // Get all functions entry points
    if( DosQueryProcAddr( m_swHandle, 0, "swLoadDriver", ( PFN * )&m_pfnSWLoadDriver))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swUnloadDriver", ( PFN * )&m_pfnSWUnloadDriver ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoOutput", ( PFN * )&m_pfnSWSetVideoOutput ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swMoveVideoOutput", ( PFN * )&m_pfnSWMoveVideoOutput ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swDisableVideoOutput", ( PFN * )&m_pfnSWDisableVideoOutput ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetDstVideoColorKey", ( PFN * )&m_pfnSWSetDstVideoColorKey ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swAllocVideoBuffers", ( PFN * )&m_pfnSWAllocVideoBuffers ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swFreeVideoBuffers", ( PFN * )&m_pfnSWFreeVideoBuffers ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swLockBuffer", ( PFN * )&m_pfnSWLockBuffer ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swUnlockBuffer", ( PFN * )&m_pfnSWUnlockBuffer ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swCaps", ( PFN * )&m_pfnSWCaps ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swQueryVideoBrightness", ( PFN * )&m_pfnSWQueryVideoBrightness ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoBrightness", ( PFN * )&m_pfnSWSetVideoBrightness ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swQueryVideoContrast", ( PFN * )&m_pfnSWQueryVideoContrast ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoContrast", ( PFN * )&m_pfnSWSetVideoContrast ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swQueryVideoSaturation", ( PFN * )&m_pfnSWQueryVideoSaturation ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoSaturation", ( PFN * )&m_pfnSWSetVideoSaturation ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swQueryVideoHue", ( PFN * )&m_pfnSWQueryVideoHue ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoHue", ( PFN * )&m_pfnSWSetVideoHue ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swQueryVideoGammaCorrect", ( PFN * )&m_pfnSWQueryVideoGammaCorrect ))
        goto exit_error;

    if( DosQueryProcAddr( m_swHandle, 0, "swSetVideoGammaCorrect", ( PFN * )&m_pfnSWSetVideoGammaCorrect ))
        goto exit_error;

    return TRUE;

exit_error :
    DosFreeModule( m_swHandle );
    m_swHandle = NULLHANDLE;

    return FALSE;
}

static MRESULT EXPENTRY snapNewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_ERASEBACKGROUND :
        {
            HPS     hpsFrame = ( HPS )mp1;
            PRECTL  prcl = ( PRECTL )mp2;

            GpiCreateLogColorTable( hpsFrame, 0, LCOLF_RGB, 0, 0, NULL );
            WinFillRect( hpsFrame, prcl, g_ulKeyColor);

            return FALSE;
        }

        case WM_SIZE :
        case WM_MOVE :
            if( m_pVideoBuf )
            {
                RECTL   rclSrc;
                RECTL   rclDst;

                rclSrc.xLeft = m_ss.lSrcX;
                rclSrc.yBottom = m_ss.lSrcY;
                rclSrc.xRight = rclSrc.xLeft + m_ss.lSrcCX;
                rclSrc.yTop = rclSrc.yBottom + m_ss.lSrcCY;
                kvaAdjustDstRect( &rclSrc, &rclDst );
                WinMapWindowPoints( hwnd, HWND_DESKTOP, ( PPOINTL )&rclDst, 2 );

                // SNAP does not like empty rect
                if( rclDst.xLeft == rclDst.xRight || rclDst.yTop == rclDst.yBottom )
                {
                    // workaround
                    rclDst.xRight = rclDst.xLeft + 1;
                    rclDst.yTop = rclDst.yBottom + 1;
                }

                m_pfnSWMoveVideoOutput( m_ss.lSrcX, m_ss.lSrcY, m_ss.lSrcCX, m_ss.lSrcCY,
                                        rclDst.xLeft, rclDst.yTop, rclDst.xRight - rclDst.xLeft, rclDst.yTop - rclDst.yBottom );
            }
            break; // fall through to old window proc
    }

    return m_pfnwpOld( hwnd, msg, mp1, mp2 );
}

APIRET APIENTRY snapInit( VOID )
{
    CHAR        szClassName[ 80 ];
    CLASSINFO   ci;
    ULONG       rc;

    memset( &m_ss, 0, sizeof( SNAPSETUP ));

    m_swHandle = NULLHANDLE;

    m_pVideoBuf = NULL;

    m_pfnwpOld = NULL;

    rc = KVAE_NO_ERROR;

    if( !LoadWrapper())
        return KVAE_CANNOT_LOAD_SNAP_WRAPPER;

    if( m_pfnSWLoadDriver())
    {
        rc = KVAE_CANNOT_LOAD_SNAP_DRIVER;
        goto exit_error;
    }

    WinQueryClassName( g_hwndKVA, sizeof( szClassName ), szClassName );
    WinQueryClassInfo( WinQueryAnchorBlock( g_hwndKVA ), szClassName, &ci );

    if( !( ci.flClassStyle & CS_MOVENOTIFY ))
    {
        ci.flClassStyle |= CS_MOVENOTIFY;

        WinRegisterClass( WinQueryAnchorBlock( g_hwndKVA ),
                          szClassName,
                          ci.pfnWindowProc,
                          ci.flClassStyle,
                          ci.cbWindowData );
    }

    m_pfnwpOld = WinSubclassWindow( g_hwndKVA, snapNewWindowProc );

    if( m_pfnwpOld )
    {
        g_pfnDone = snapDone;
        g_pfnLockBuffer = snapLockBuffer;
        g_pfnUnlockBuffer = snapUnlockBuffer;
        g_pfnSetup = snapSetup;
        g_pfnCaps = snapCaps;
        g_pfnQueryAttr = snapQueryAttr;
        g_pfnSetAttr = snapSetAttr;
    }
    else
    {
        rc = KVAE_CANNOT_SUBCLASS;
        goto exit_subclass;
    }

    return KVAE_NO_ERROR;

exit_subclass :
    m_pfnSWUnloadDriver();

exit_error :
    DosFreeModule( m_swHandle );
    m_swHandle = NULLHANDLE;

    return rc;
}

static APIRET APIENTRY snapDone( VOID )
{
    WinSubclassWindow( g_hwndKVA, m_pfnwpOld );

    m_pfnSWDisableVideoOutput();

    if( m_pVideoBuf )
    {
        m_pfnSWFreeVideoBuffers( m_pVideoBuf );
        m_pVideoBuf = NULL;
    }

    m_pfnSWUnloadDriver();

    return KVAE_NO_ERROR;
}

static APIRET APIENTRY snapLockBuffer( PPVOID ppBuffer, PULONG pulBPL )
{
    m_pfnSWLockBuffer( m_pVideoBuf, ppBuffer, pulBPL );

    return KVAE_NO_ERROR;
}

static APIRET APIENTRY snapUnlockBuffer( VOID )
{
    m_pfnSWUnlockBuffer( m_pVideoBuf );

    return KVAE_NO_ERROR;
}

static APIRET APIENTRY snapSetup( PKVASETUP pkvas )
{
    RECTL rclDst;

    if( m_pVideoBuf )
        m_pfnSWFreeVideoBuffers( m_pVideoBuf );

    if( m_pfnSWAllocVideoBuffers( &m_pVideoBuf, pkvas->szlSrcSize.cx, pkvas->szlSrcSize.cy, pkvas->fccSrcColor, 1 ))
    {
        m_pVideoBuf = NULL;

        return KVAE_CANNOT_ALLOC_VIDEO_BUFFER;
    }

    m_pfnSWSetDstVideoColorKey( g_ulKeyColor );

    m_ss.lSrcX = 0;
    m_ss.lSrcY = 0;
    m_ss.lSrcCX = pkvas->szlSrcSize.cx;
    m_ss.lSrcCY = pkvas->szlSrcSize.cy;

    kvaAdjustDstRect( &pkvas->rclSrcRect, &rclDst );
    WinMapWindowPoints( g_hwndKVA, HWND_DESKTOP, ( PPOINTL )&rclDst, 2 );

    // SNAP does not like empty rect
    if( rclDst.xLeft == rclDst.xRight || rclDst.yTop == rclDst.yBottom )
    {
        // workaround
        rclDst.xRight = rclDst.xLeft + 1;
        rclDst.yTop = rclDst.yBottom + 1;
    }

    if( m_pfnSWSetVideoOutput( m_pVideoBuf,
                               m_ss.lSrcX, m_ss.lSrcY, m_ss.lSrcCX, m_ss.lSrcCY,
                               rclDst.xLeft, rclDst.yTop, rclDst.xRight - rclDst.xLeft, rclDst.yTop - rclDst.yBottom,
                               pkvas->fccSrcColor ))
    {
        m_pfnSWFreeVideoBuffers( m_pVideoBuf );

        m_pVideoBuf = NULL;

        return KVAE_CANNOT_SETUP;
    }

    return KVAE_NO_ERROR;
}

static APIRET APIENTRY snapCaps( PKVACAPS pkvac )
{
    m_pfnSWCaps( pkvac );

    return KVAE_NO_ERROR;
}

APIRET APIENTRY snapQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    APIRET rc = -1;

    switch( ulAttr )
    {
        case KVAA_BRIGHTNESS :
            rc = m_pfnSWQueryVideoBrightness( pulValue );
            break;

        case KVAA_CONTRAST :
            rc = m_pfnSWQueryVideoContrast( pulValue );
            break;

        case KVAA_SATURATION :
            rc = m_pfnSWQueryVideoSaturation( pulValue );
            break;

        case KVAA_HUE :
            rc = m_pfnSWQueryVideoHue( pulValue );
            break;

        case KVAA_GAMMA :
            rc = m_pfnSWQueryVideoGammaCorrect( pulValue );
            break;
    }

    if( rc )
        return KVAE_NO_ATTRIBUTE;

    return KVAE_NO_ERROR;
}

APIRET APIENTRY snapSetAttr( ULONG ulAttr, PULONG pulValue )
{
    APIRET rc = -1;

    switch( ulAttr )
    {
        case KVAA_BRIGHTNESS :
            rc = m_pfnSWSetVideoBrightness( pulValue );
            break;

        case KVAA_CONTRAST :
            rc = m_pfnSWSetVideoContrast( pulValue );
            break;

        case KVAA_SATURATION :
            rc = m_pfnSWSetVideoSaturation( pulValue );
            break;

        case KVAA_HUE :
            rc = m_pfnSWSetVideoHue( pulValue );
            break;

        case KVAA_GAMMA :
            rc = m_pfnSWSetVideoGammaCorrect( pulValue );
            break;
    }

    if( rc )
        return KVAE_NO_ATTRIBUTE;

    return KVAE_NO_ERROR;
}
