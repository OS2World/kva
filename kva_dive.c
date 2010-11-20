/*
    DIVE interface for K Video Accelerator
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
*/

#define INCL_WIN
#define INCL_GPI
#define INCL_DOS
#include <os2.h>

#include <mmioos2.h>
#include <fourcc.h>
#include <dive.h>

#include <stdlib.h>
#include <string.h>

#include "kva.h"
#include "kva_internal.h"
#include "kva_dive.h"

static HDIVE            m_hdive = NULLHANDLE;
static ULONG            m_ulBufferNumber = 0;
static SETUP_BLITTER    m_sb = { 0 };

static PFNWP        m_pfnwpOld = NULL;

static APIRET APIENTRY diveDone( VOID );
static APIRET APIENTRY diveLockBuffer( PPVOID ppBuffer, PULONG pulBPL );
static APIRET APIENTRY diveUnlockBuffer( VOID );
static APIRET APIENTRY diveSetup( PKVASETUP pkvas );
static APIRET APIENTRY diveCaps( PKVACAPS pkvac );
static APIRET APIENTRY diveQueryAttr( ULONG ulAttr, PULONG pulValue );
static APIRET APIENTRY diveSetAttr( ULONG ulAttr, PULONG pulValue );

static HMODULE m_hmodDive = NULLHANDLE;

static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveQueryCaps, ( PDIVE_CAPS, ULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveOpen, ( HDIVE *, BOOL, PVOID ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveClose, ( HDIVE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveSetupBlitter, ( HDIVE, PSETUP_BLITTER ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveBlitImage, ( HDIVE, ULONG, ULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveSetDestinationPalette, ( HDIVE, ULONG, ULONG, PBYTE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveAllocImageBuffer, ( HDIVE, PULONG, FOURCC, ULONG, ULONG, ULONG, PBYTE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveFreeImageBuffer, ( HDIVE, ULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveBeginImageBufferAccess, ( HDIVE, ULONG, PBYTE *, PULONG, PULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveEndImageBufferAccess, ( HDIVE, ULONG ));

static BOOL loadDive( VOID )
{
    UCHAR szFailedName[ 256 ];

    if( DosLoadModule( szFailedName, sizeof( szFailedName ), "DIVE", &m_hmodDive ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 1, NULL, ( PFN * )&m_pfnDiveQueryCaps ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 2, NULL, ( PFN * )&m_pfnDiveOpen ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 3, NULL, ( PFN * )&m_pfnDiveClose ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 4, NULL, ( PFN * )&m_pfnDiveSetupBlitter ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 5, NULL, ( PFN * )&m_pfnDiveBlitImage ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 9, NULL, ( PFN * )&m_pfnDiveSetDestinationPalette ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 12, NULL, ( PFN * )&m_pfnDiveAllocImageBuffer ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 13, NULL, ( PFN * )&m_pfnDiveFreeImageBuffer ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 14, NULL, ( PFN * )&m_pfnDiveBeginImageBufferAccess ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 15, NULL, ( PFN * )&m_pfnDiveEndImageBufferAccess ))
        return FALSE;

    return TRUE;
}

static APIRET destSetup( VOID )
{
    HPS             hps;
    HRGN            hrgn;
    RGNRECT         rgnCtl;
    PRECTL          prcl = NULL;
    ULONG           rc = KVAE_NO_ERROR;

    hps = WinGetPS( g_hwndKVA );

    hrgn = GpiCreateRegion( hps, 0L, NULL );
    if( hrgn )
    {
        WinQueryVisibleRegion( g_hwndKVA, hrgn );

        rgnCtl.ircStart     = 1;
        rgnCtl.ulDirection  = RECTDIR_LFRT_TOPBOT;
        GpiQueryRegionRects( hps, hrgn, NULL, &rgnCtl, NULL );

        if( rgnCtl.crcReturned > 0 )
        {
           rgnCtl.crc = rgnCtl.crcReturned;
           prcl = malloc( sizeof( RECTL ) * rgnCtl.crcReturned );
        }

        /* Get the all ORed rectangles
        */
        if( prcl && GpiQueryRegionRects( hps, hrgn, NULL, &rgnCtl, prcl ))
        {
            SWP     swp;
            POINTL  ptl;
            RECTL   rclSrc, rclDst;

            WinQueryWindowPos( g_hwndKVA, &swp );

            ptl.x = swp.x;
            ptl.y = swp.y;

            WinMapWindowPoints( WinQueryWindow( g_hwndKVA, QW_PARENT ),
                                HWND_DESKTOP,
                                &ptl, 1 );

            rclSrc.xLeft = 0;
            rclSrc.yBottom = 0;
            rclSrc.xRight = rclSrc.xLeft + m_sb.ulSrcWidth;
            rclSrc.yTop = rclSrc.yBottom + m_sb.ulSrcHeight;

            kvaAdjustDstRect( &rclSrc, &rclDst );
            WinMapWindowPoints( g_hwndKVA, HWND_DESKTOP, ( PPOINTL )&rclDst, 2 );

            // Tell DIVE about the new settings.

            m_sb.fccDstColorFormat = FOURCC_SCRN;
            m_sb.ulDstWidth        = rclDst.xRight - rclDst.xLeft;
            m_sb.ulDstHeight       = rclDst.yTop - rclDst.yBottom;
            m_sb.lDstPosX          = rclDst.xLeft - ptl.x;
            m_sb.lDstPosY          = rclDst.yBottom - ptl.y;
            m_sb.lScreenPosX       = ptl.x;
            m_sb.lScreenPosY       = ptl.y;
            m_sb.ulNumDstRects     = rgnCtl.crcReturned;
            m_sb.pVisDstRects      = prcl;

            rc = m_pfnDiveSetupBlitter( m_hdive, &m_sb );
        }
        else
            rc = m_pfnDiveSetupBlitter( m_hdive, 0 );

        free( prcl );

        GpiDestroyRegion( hps, hrgn );
    }

    WinReleasePS( hps );

    return rc;
}

static MRESULT EXPENTRY diveNewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
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

        case WM_VRNDISABLED :
            m_pfnDiveSetupBlitter( m_hdive, 0 );

            return 0;

        case WM_VRNENABLED :
            destSetup();

            return 0;

        case WM_REALIZEPALETTE :
            m_pfnDiveSetDestinationPalette ( m_hdive, 0, 0, 0 );

            return 0;

        // workaround for Mozilla plugin
        case WM_MOVE :
        case WM_PAINT :
            destSetup();

            break;  // fall through to old window proc
    }

    return m_pfnwpOld( hwnd, msg, mp1, mp2 );
}

APIRET APIENTRY diveInit( VOID )
{
    ULONG   rc;

    m_hmodDive = NULLHANDLE;

    if( !loadDive())
    {
        rc = KVAE_CANNOT_LOAD_DIVE;
        goto exit_error;
    }

    m_hdive = NULLHANDLE;
    m_ulBufferNumber = 0;
    memset( &m_sb, 0, sizeof( SETUP_BLITTER ));

    m_pfnwpOld = NULL;

    rc = m_pfnDiveOpen( &m_hdive, FALSE, 0 );
    if( rc )
        goto exit_error;

    m_pfnwpOld = WinSubclassWindow( g_hwndKVA, diveNewWindowProc );

    if( m_pfnwpOld )
    {
        g_pfnDone = diveDone;
        g_pfnLockBuffer = diveLockBuffer;
        g_pfnUnlockBuffer = diveUnlockBuffer;
        g_pfnSetup = diveSetup;
        g_pfnCaps = diveCaps;
        g_pfnQueryAttr = diveQueryAttr;
        g_pfnSetAttr = diveSetAttr;

        WinSetVisibleRegionNotify( g_hwndKVA, TRUE );
    }
    else
    {
        rc = KVAE_CANNOT_SUBCLASS;
        goto exit_error;
    }

    return KVAE_NO_ERROR;

exit_error:
    if( m_hmodDive )
    {
        if( m_hdive )
            m_pfnDiveClose( m_hdive );

        DosFreeModule( m_hmodDive );
    }

    return rc;
}

static APIRET APIENTRY diveDone( VOID )
{
    ULONG rc;

    WinSetVisibleRegionNotify( g_hwndKVA, FALSE );

    WinSubclassWindow( g_hwndKVA, m_pfnwpOld );

    rc = m_pfnDiveClose( m_hdive );

    DosFreeModule( m_hmodDive );

    return rc;
}

static APIRET APIENTRY diveLockBuffer( PPVOID ppBuffer, PULONG pulBPL )
{
    ULONG   ulScanLines;
    ULONG   rc;

    rc = m_pfnDiveAllocImageBuffer( m_hdive, &m_ulBufferNumber, m_sb.fccSrcColorFormat,
                                    m_sb.ulSrcWidth, m_sb.ulSrcHeight, 0, 0 );
    if( rc )
        return rc;

    rc = m_pfnDiveBeginImageBufferAccess( m_hdive, m_ulBufferNumber, ( PBYTE * )ppBuffer, pulBPL, &ulScanLines );
    if( rc )
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulBufferNumber );

    return rc;
}

static APIRET APIENTRY diveUnlockBuffer( VOID )
{
    ULONG rc, rc1;

    rc = m_pfnDiveEndImageBufferAccess( m_hdive, m_ulBufferNumber );
    if( rc )
        goto exit;

    rc = m_pfnDiveBlitImage( m_hdive, m_ulBufferNumber, DIVE_BUFFER_SCREEN );
    if( rc == DIVE_ERR_BLITTER_NOT_SETUP ) // occur when entirely covered
        rc = DIVE_SUCCESS;
exit:
    rc1 = m_pfnDiveFreeImageBuffer( m_hdive, m_ulBufferNumber );
    if( rc )
        rc1 = rc;

    return rc1;
}

static APIRET APIENTRY diveSetup( PKVASETUP pkvas )
{
    m_sb.ulStructLen       = sizeof( SETUP_BLITTER );
    m_sb.fccSrcColorFormat = pkvas->fccSrcColor;
    m_sb.ulSrcWidth        = pkvas->szlSrcSize.cx;
    m_sb.ulSrcHeight       = pkvas->szlSrcSize.cy;
    m_sb.ulSrcPosX         = 0;
    m_sb.ulSrcPosY         = 0;
    m_sb.fInvert           = pkvas->fInvert;
    m_sb.ulDitherType      = ( ULONG )pkvas->fDither;

    return destSetup();
}

#ifndef SHOW_CPAS

static APIRET APIENTRY diveCaps( PKVACAPS pkvac )
{
    FOURCC      fccFormats[ 100 ];
    DIVE_CAPS   diveCaps;
    BYTE        bRLen, bROfs, bGLen, bGOfs, bBLen, bBOfs;
    ULONG       rc;

    diveCaps.ulStructLen = sizeof( DIVE_CAPS );
    diveCaps.pFormatData = fccFormats;
    diveCaps.ulFormatLength = sizeof( fccFormats );

    rc = m_pfnDiveQueryCaps( &diveCaps, DIVE_BUFFER_SCREEN );
    if( rc )
        return rc;

    pkvac->ulMode = KVAM_DIVE;
    pkvac->ulDepth = diveCaps.ulDepth;
    pkvac->cxScreen = diveCaps.ulHorizontalResolution;
    pkvac->cyScreen = diveCaps.ulVerticalResolution;
    pkvac->fccScreen = diveCaps.fccColorEncoding;

    pkvac->ulInputFormatFlags = 0;
    switch( pkvac->fccScreen )
    {
        case FOURCC_R555 :
            pkvac->ulInputFormatFlags |= KVAF_BGR15;
            bRLen = 5;
            bROfs = 10;
            bGLen = 5;
            bGOfs = 5;
            bBLen = 5;
            bBOfs = 0;
            break;

        case FOURCC_R565 :
            pkvac->ulInputFormatFlags |= KVAF_BGR16;
            bRLen = 5;
            bROfs = 11;
            bGLen = 6;
            bGOfs = 5;
            bBLen = 5;
            bBOfs = 0;
            break;

        case FOURCC_BGR4 :
        case FOURCC_BGR3 :
        case FOURCC_LUT8 :  // maybe best T.T
        default :
            pkvac->ulInputFormatFlags |= KVAF_BGR24;
            bRLen = 8;
            bROfs = 16;
            bGLen = 8;
            bGOfs = 8;
            bBLen = 8;
            bBOfs = 0;
            break;
    }

    pkvac->ulRMask = (( 1 << bRLen ) - 1 ) << bROfs;
    pkvac->ulGMask = (( 1 << bGLen ) - 1 ) << bGOfs;
    pkvac->ulBMask = (( 1 << bBLen ) - 1 ) << bBOfs;

    return KVAE_NO_ERROR;
}
#else
#include <stdio.h>

static APIRET APIENTRY diveCaps( PKVACAPS pkvac )
{
    FOURCC      fccFormats[ 100 ];
    DIVE_CAPS   diveCaps;
    int         i;

    diveCaps.ulStructLen = sizeof( DIVE_CAPS );
    diveCaps.pFormatData = fccFormats;
    diveCaps.ulFormatLength = sizeof( fccFormats );
    if( m_pfnDiveQueryCaps( &diveCaps, DIVE_BUFFER_SCREEN ))
    {
        printf("DiveQueryCaps error\n");
        return KVAE_NO_ERROR;
    }

    printf("--- DiveCaps ---\n");
    printf("ulPlaneCount = %ld\n", diveCaps.ulPlaneCount );
    printf("fScreenDirect = %ld\n", diveCaps.fScreenDirect );
    printf("fBankSwitched = %ld\n", diveCaps.fBankSwitched );
    printf("ulDepth = %ld\n", diveCaps.ulDepth );
    printf("ulHorizontalResolution = %ld\n", diveCaps.ulHorizontalResolution );
    printf("ulVerticalResolution = %ld\n", diveCaps.ulVerticalResolution );
    printf("ulScanLineBytes = %ld\n", diveCaps.ulScanLineBytes );
    printf("fccColorEncoding = %.4s\n", &diveCaps.fccColorEncoding );
    printf("ulApertureSize = %ld\n", diveCaps.ulApertureSize );
    printf("ulInputFormats = %ld\n", diveCaps.ulInputFormats );
    printf("ulOutputFormats = %ld\n", diveCaps.ulOutputFormats );
    printf("ulFormatLength = %ld\n", diveCaps.ulFormatLength );
    for( i = 0; i < diveCaps.ulFormatLength / sizeof( FOURCC ); i++ )
        printf("%dth format data = %.4s\n", i, (( FOURCC * )diveCaps.pFormatData ) + i );

    return KVAE_NO_ERROR;
}
#endif

APIRET APIENTRY diveQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    return KVAE_NO_ATTRIBUTE;
}

APIRET APIENTRY diveSetAttr( ULONG ulAttr, PULONG pulValue )
{
    return KVAE_NO_ATTRIBUTE;
}

