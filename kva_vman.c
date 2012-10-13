/*
    VMAN interface for K Video Accelerator
    Copyright (C) 2012 by KO Myung-Hun <komh@chollian.net>

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

#include <dive.h>

#include <stdlib.h>
#include <string.h>

#undef FOURCC
#include "gradd.h"

#include "kva.h"
#include "kva_internal.h"
#include "kva_vman.h"

#define calcStride( width, fcc ) ((( width ) * fcc2bpp( fcc ) + 7 ) & ~7 )

static GDDMODEINFO  m_mi;

static BOOL m_isVRNEnabled  = TRUE;
static BOOL m_isBlitAllowed = FALSE;

static BLTRECT  m_brSrc;
static BMAPINFO m_bmiDst;
static BLTRECT  m_brDst;

static LONG   m_lVisibleRects    = 0;
static PRECTL m_prclVisibleRects = NULL;

static PFNWP m_pfnwpOld = NULL;

static APIRET APIENTRY vmanDone( VOID );
static APIRET APIENTRY vmanLockBuffer( PPVOID ppBuffer, PULONG pulBPL );
static APIRET APIENTRY vmanUnlockBuffer( VOID );
static APIRET APIENTRY vmanSetup( PKVASETUP pkvas );
static APIRET APIENTRY vmanCaps( PKVACAPS pkvac );
static APIRET APIENTRY vmanQueryAttr( ULONG ulAttr, PULONG pulValue );
static APIRET APIENTRY vmanSetAttr( ULONG ulAttr, PULONG pulValue );

static HMODULE m_hmodVman = NULLHANDLE;

static FNVMIENTRY *m_pfnVMIEntry;

static HMODULE m_hmodDive = NULLHANDLE;

static HDIVE         m_hdive       = NULLHANDLE;
static ULONG         m_ulSrcBufNum = 0;
static ULONG         m_ulDstBufNum = 0;
static PBYTE         m_pbSrcImgBuf;
static PBYTE         m_pbDstImgBuf;
static SETUP_BLITTER m_sb;

static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveOpen, ( HDIVE *, BOOL, PVOID ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveClose, ( HDIVE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveSetupBlitter, ( HDIVE, PSETUP_BLITTER ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveBlitImage, ( HDIVE, ULONG, ULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveAllocImageBuffer, ( HDIVE, PULONG, FOURCC, ULONG, ULONG, ULONG, PBYTE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveFreeImageBuffer, ( HDIVE, ULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveBeginImageBufferAccess, ( HDIVE, ULONG, PBYTE *, PULONG, PULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnDiveEndImageBufferAccess, ( HDIVE, ULONG ));

static BOOL loadVman( VOID )
{
    UCHAR szFailedName[ 256 ];

    if( DosLoadModule( szFailedName, sizeof( szFailedName ), "VMAN", &m_hmodVman ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodVman, 0, "VMIEntry", ( PFN * )&m_pfnVMIEntry ))
        return FALSE;

    return TRUE;
}

static BOOL loadDive( VOID )
{
    UCHAR szFailedName[ 256 ];

    if( DosLoadModule( szFailedName, sizeof( szFailedName ), "DIVE", &m_hmodDive ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 2, NULL, ( PFN * )&m_pfnDiveOpen ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 3, NULL, ( PFN * )&m_pfnDiveClose ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 4, NULL, ( PFN * )&m_pfnDiveSetupBlitter ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodDive, 5, NULL, ( PFN * )&m_pfnDiveBlitImage ))
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

static int fcc2bpp( FOURCC fcc )
{
    switch( fcc )
    {
        case FOURCC_LUT8 :
            return 1;

        case FOURCC_R555 :
        case FOURCC_R565 :
            return 2;

        case FOURCC_BGR3 :
            return 3;

        case FOURCC_BGR4 :
            return 4;
    }

    // Ooops, expect a luck
    return 2;
}

static APIRET destSetup( VOID )
{
    HPS     hps;
    HRGN    hrgn;
    RGNRECT rgnCtl;
    PRECTL  prcl = NULL;
    ULONG   rc = KVAE_NO_ERROR;

    m_isBlitAllowed = FALSE;

    m_lVisibleRects = 0;

    free( m_prclVisibleRects );
    m_prclVisibleRects = NULL;

    if( m_ulDstBufNum != 0 )
    {
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulDstBufNum );

        free( m_pbDstImgBuf );

        m_ulDstBufNum = 0;
    }

    hps = WinGetPS( g_hwndKVA );

    hrgn = GpiCreateRegion( hps, 0L, NULL );
    if( hrgn )
    {
        RECTL rclSrc, rclDst;
        HRGN  hrgnDst;

        WinQueryVisibleRegion( g_hwndKVA, hrgn );

        rclSrc.xLeft   = m_sb.ulSrcPosX;
        rclSrc.yBottom = m_sb.ulSrcPosY;
        rclSrc.xRight  = rclSrc.xLeft   + m_sb.ulSrcWidth;
        rclSrc.yTop    = rclSrc.yBottom + m_sb.ulSrcHeight;
        kvaAdjustDstRect( &rclSrc, &rclDst );

        hrgnDst = GpiCreateRegion( hps, 1, &rclDst );
        GpiCombineRegion( hps, hrgn, hrgn, hrgnDst, CRGN_AND );
        GpiDestroyRegion( hps, hrgnDst );

        rgnCtl.ircStart     = 1;
        rgnCtl.ulDirection  = RECTDIR_LFRT_TOPBOT;
        GpiQueryRegionRects( hps, hrgn, NULL, &rgnCtl, NULL );

        if( rgnCtl.crcReturned > 0 )
        {
           rgnCtl.crc = rgnCtl.crcReturned;
           prcl       = malloc( sizeof( RECTL ) * rgnCtl.crcReturned );
        }

        /* Get the all ORed rectangles
        */
        if( prcl && GpiQueryRegionRects( hps, hrgn, NULL, &rgnCtl, prcl ))
        {
            WinMapWindowPoints( g_hwndKVA, HWND_DESKTOP,
                                ( PPOINTL )prcl, rgnCtl.crcReturned * 2 );

            WinMapWindowPoints( g_hwndKVA, HWND_DESKTOP,
                                ( PPOINTL )&rclDst, 2 );

            m_brDst.ulXOrg = rclDst.xLeft;
            m_brDst.ulYOrg = m_mi.ulVertResolution - rclDst.yTop; // Invert Y
            m_brDst.ulXExt = rclDst.xRight - rclDst.xLeft;
            m_brDst.ulYExt = rclDst.yTop - rclDst.yBottom;

            // Need color space conversion or scaling ?
            if( m_sb.fccSrcColorFormat != m_mi.fccColorEncoding ||
                m_sb.ulSrcWidth        != m_brDst.ulXExt ||
                m_sb.ulSrcHeight       != m_brDst.ulYExt )
            {
                rclDst.xLeft   = 0;
                rclDst.yBottom = 0;
                rclDst.xRight  = m_brDst.ulXExt;
                rclDst.yTop    = m_brDst.ulYExt;

                m_sb.fccDstColorFormat = m_mi.fccColorEncoding;
                m_sb.ulDstWidth        = m_brDst.ulXExt;
                m_sb.ulDstHeight       = m_brDst.ulYExt;
                m_sb.lDstPosX          = 0;
                m_sb.lDstPosY          = 0;
                m_sb.lScreenPosX       = 0;
                m_sb.lScreenPosY       = 0;
                m_sb.ulNumDstRects     = 1;
                m_sb.pVisDstRects      = &rclDst;

                rc = m_pfnDiveSetupBlitter( m_hdive, &m_sb );
                if( !rc )
                {
                    ULONG ulStride = calcStride( m_sb.ulDstWidth,
                                                 m_sb.fccDstColorFormat );

                    m_pbDstImgBuf = malloc( ulStride * m_sb.ulDstHeight );

                    m_pfnDiveAllocImageBuffer( m_hdive, &m_ulDstBufNum,
                                               m_sb.fccDstColorFormat,
                                               m_sb.ulDstWidth,
                                               m_sb.ulDstHeight,
                                               ulStride,
                                               m_pbDstImgBuf );
                }
            }

            m_isBlitAllowed = !rc;
        }

        GpiDestroyRegion( hps, hrgn );

        m_lVisibleRects    = rgnCtl.crcReturned;
        m_prclVisibleRects = prcl;
    }

    WinReleasePS( hps );

    return rc;
}

static MRESULT EXPENTRY vmanNewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
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
            m_isVRNEnabled = FALSE;

            return 0;

        case WM_VRNENABLED :
            destSetup();
            m_isVRNEnabled = TRUE;

            return 0;

        case WM_REALIZEPALETTE :
            return 0;

        // Sometimes WM_VRNENABLED is not sent, instead only WM_VRNDISABLED
        // is sent. So an image is not updated. To work around this, set
        // m_isVRNEnabled to TRUE when receiving WM_SIZE, WM_MOVE and
        // WM_PAINT as well as WM_VRNENABLED
        case WM_SIZE :
        // workaround for Mozilla plugin
        case WM_MOVE :
        case WM_PAINT :
            destSetup();
            m_isVRNEnabled = TRUE;

            break;  // fall through to old window proc
    }

    return m_pfnwpOld( hwnd, msg, mp1, mp2 );
}

APIRET APIENTRY vmanInit( VOID )
{
    INITPROCOUT ipc;
    ULONG       rc;

    m_hmodVman = NULLHANDLE;

    if( !loadVman())
    {
        rc = KVAE_CANNOT_LOAD_VMAN;

        goto exit_free_vman;
    }

    m_hmodDive = NULLHANDLE;

    if( !loadDive())
    {
        rc = KVAE_CANNOT_LOAD_DIVE;

        goto exit_free_dive;
    }

    m_isVRNEnabled  = TRUE;
    m_isBlitAllowed = FALSE;

    m_lVisibleRects    = 0;
    m_prclVisibleRects = NULL;

    m_pfnwpOld = NULL;

    ipc.ulLength = sizeof( ipc );
    rc = m_pfnVMIEntry( 0, VMI_CMD_INITPROC, NULL, &ipc );
    if( rc )
        goto exit_free_dive;

    rc = m_pfnVMIEntry( 0, VMI_CMD_QUERYCURRENTMODE, NULL, &m_mi );
    if( rc )
        goto exit_term_proc;

    m_bmiDst.ulLength       = sizeof( BMAPINFO );
    m_bmiDst.ulType         = BMAP_VRAM;
    m_bmiDst.ulWidth        = m_mi.ulHorizResolution;
    m_bmiDst.ulHeight       = m_mi.ulVertResolution;
    m_bmiDst.ulBpp          = m_mi.ulBpp;
    m_bmiDst.ulBytesPerLine = m_mi.ulScanLineSize;
    m_bmiDst.pBits          = ( PBYTE )ipc.ulVRAMVirt;

    m_hdive = NULLHANDLE;
    rc = m_pfnDiveOpen( &m_hdive, TRUE, 0 );
    if( rc )
        goto exit_term_proc;

    m_pfnwpOld = WinSubclassWindow( g_hwndKVA, vmanNewWindowProc );

    if( m_pfnwpOld )
    {
        g_pfnDone = vmanDone;
        g_pfnLockBuffer = vmanLockBuffer;
        g_pfnUnlockBuffer = vmanUnlockBuffer;
        g_pfnSetup = vmanSetup;
        g_pfnCaps = vmanCaps;
        g_pfnQueryAttr = vmanQueryAttr;
        g_pfnSetAttr = vmanSetAttr;

        WinSetVisibleRegionNotify( g_hwndKVA, TRUE );
    }
    else
    {
        rc = KVAE_CANNOT_SUBCLASS;
        goto exit_dive_close;
    }

    return KVAE_NO_ERROR;

exit_dive_close :
    m_pfnDiveClose( m_hdive );

exit_term_proc :
    m_pfnVMIEntry( 0, VMI_CMD_TERMPROC, NULL, NULL );

exit_free_dive :
    DosFreeModule( m_hmodDive );

exit_free_vman :
    DosFreeModule( m_hmodVman );

    return rc;
}

static APIRET APIENTRY vmanDone( VOID )
{
    WinSetVisibleRegionNotify( g_hwndKVA, FALSE );

    WinSubclassWindow( g_hwndKVA, m_pfnwpOld );

    m_lVisibleRects = 0;

    free( m_prclVisibleRects );
    m_prclVisibleRects = NULL;

    if( m_ulDstBufNum != 0 )
    {
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulDstBufNum );

        free( m_pbDstImgBuf );

        m_ulDstBufNum = 0;
    }

    if( m_ulSrcBufNum != 0 )
    {
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulSrcBufNum );

        free( m_pbSrcImgBuf );

        m_ulSrcBufNum = 0;
    }

    m_pfnDiveClose( m_hdive );

    m_pfnVMIEntry( 0, VMI_CMD_TERMPROC, NULL, NULL );

    DosFreeModule( m_hmodDive );
    DosFreeModule( m_hmodVman );

    return KVAE_NO_ERROR;
}

static APIRET APIENTRY vmanLockBuffer( PPVOID ppBuffer, PULONG pulBPL )
{
    ULONG ulScanLines;

    return m_pfnDiveBeginImageBufferAccess( m_hdive, m_ulSrcBufNum,
                                            ppBuffer, pulBPL, &ulScanLines );
}

static APIRET APIENTRY vmanUnlockBuffer( VOID )
{
    PBYTE      pBits;
    ULONG      ulBytesPerLine;
    ULONG      ulWidth, ulHeight;
    ULONG      ulSrcPosX, ulSrcPosY;
    BMAPINFO   bmiSrc;
    RECTL      rclSrcBounds;
    RECTL      rclDstBounds;
    PPOINTL    pptlSrcOrg;
    PBLTRECT   pbrDst;
    BITBLTINFO bbi;
    HWREQIN    hri;
    int        i;
    HAB        hab;
    ULONG      rc = KVAE_NO_ERROR;

    if( !m_isVRNEnabled || !m_isBlitAllowed )
        goto exit_end_image_src;

    if( m_ulDstBufNum != 0 )
    {
        ULONG ulScanLines;

        rc = m_pfnDiveBlitImage( m_hdive, m_ulSrcBufNum, m_ulDstBufNum );
        if( rc )
            goto exit_end_image_src;

        rc = m_pfnDiveBeginImageBufferAccess( m_hdive, m_ulDstBufNum,
                                              &pBits, &ulBytesPerLine,
                                              &ulScanLines );
        if( rc )
            goto exit_end_image_src;

        ulWidth  = m_brDst.ulXExt;
        ulHeight = m_brDst.ulYExt;

        ulSrcPosX = 0;
        ulSrcPosY = 0;
    }
    else
    {
        ulBytesPerLine = calcStride( m_brSrc.ulXExt, m_sb.fccSrcColorFormat );
        pBits          = m_pbSrcImgBuf;

        ulWidth  = m_brSrc.ulXExt;
        ulHeight = m_brSrc.ulYExt;

        ulSrcPosX = m_sb.ulSrcPosX;
        ulSrcPosY = m_sb.ulSrcPosY;
    }

    bmiSrc.ulLength       = sizeof( BMAPINFO );
    bmiSrc.ulType         = BMAP_MEMORY;
    bmiSrc.ulWidth        = ulWidth;
    bmiSrc.ulHeight       = ulHeight;
    bmiSrc.ulBpp          = m_mi.ulBpp;
    bmiSrc.ulBytesPerLine = ulBytesPerLine;
    bmiSrc.pBits          = pBits;

    rclSrcBounds.xLeft   = ulSrcPosX;
    rclSrcBounds.yTop    = ulHeight - ulSrcPosY;
    rclSrcBounds.xRight  = rclSrcBounds.xLeft + m_brDst.ulXExt;
    rclSrcBounds.yBottom = rclSrcBounds.yTop - m_brDst.ulYExt;

    hab = WinQueryAnchorBlock( g_hwndKVA );

    WinSetRectEmpty( hab, &rclDstBounds );

    pptlSrcOrg = calloc( m_lVisibleRects, sizeof( POINTL ));
    pbrDst     = calloc( m_lVisibleRects, sizeof( BLTRECT ));

    for( i = 0; i < m_lVisibleRects; i++ )
    {
        WinUnionRect( hab, &rclDstBounds, &rclDstBounds,
                      &m_prclVisibleRects[ i ]);

        pptlSrcOrg[ i ].x = ulSrcPosX +
                            ( m_prclVisibleRects[ i ].xLeft - m_brDst.ulXOrg );
        pptlSrcOrg[ i ].y = ulSrcPosY +
                            ( m_mi.ulVertResolution -
                              m_prclVisibleRects[ i ].yTop ) - m_brDst.ulYOrg;

        pbrDst[ i ].ulXOrg = m_prclVisibleRects[ i ].xLeft;
        pbrDst[ i ].ulYOrg = m_mi.ulVertResolution -
                             m_prclVisibleRects[ i ].yTop;
        pbrDst[ i ].ulXExt = m_prclVisibleRects[ i ].xRight -
                             m_prclVisibleRects[ i].xLeft;
        pbrDst[ i ].ulYExt = m_prclVisibleRects[ i ].yTop -
                             m_prclVisibleRects[ i ].yBottom;
    }

    memset( &bbi, 0, sizeof( BITBLTINFO ));
    bbi.ulLength      = sizeof( BITBLTINFO );
    bbi.ulBltFlags    = BF_DEFAULT_STATE | BF_ROP_INCL_SRC | BF_PAT_HOLLOW;
    bbi.cBlits        = m_lVisibleRects;
    bbi.ulROP         = ROP_SRCCOPY;
    bbi.pSrcBmapInfo  = &bmiSrc;
    bbi.pDstBmapInfo  = &m_bmiDst;
    bbi.prclSrcBounds = &rclSrcBounds;
    bbi.prclDstBounds = &rclDstBounds;
    bbi.aptlSrcOrg    = pptlSrcOrg;
    bbi.abrDst        = pbrDst;

    hri.ulLength        = sizeof( HWREQIN );
    hri.ulFlags         = REQUEST_HW;
    hri.cScrChangeRects = m_lVisibleRects;
    hri.arectlScreen    = m_prclVisibleRects;

    if( !m_pfnVMIEntry( 0, VMI_CMD_REQUESTHW, &hri, NULL ))
    {
        m_pfnVMIEntry( 0, VMI_CMD_BITBLT, &bbi, NULL );

        hri.ulFlags = 0;
        m_pfnVMIEntry( 0, VMI_CMD_REQUESTHW, &hri, NULL );
    }

    free( pbrDst );
    free( pptlSrcOrg );

    if( m_ulDstBufNum != 0 )
        m_pfnDiveEndImageBufferAccess( m_hdive, m_ulDstBufNum );

exit_end_image_src :
    m_pfnDiveEndImageBufferAccess( m_hdive, m_ulSrcBufNum );

    return rc;
}

static APIRET APIENTRY vmanSetup( PKVASETUP pkvas )
{
    ULONG ulStride;
    ULONG rc;

    if( m_ulSrcBufNum != 0 )
    {
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulSrcBufNum );

        free( m_pbSrcImgBuf );

        m_ulSrcBufNum = 0;
    }

    ulStride = calcStride( pkvas->szlSrcSize.cx, pkvas->fccSrcColor );
    m_pbSrcImgBuf = malloc( ulStride * pkvas->szlSrcSize.cy );

    m_pfnDiveAllocImageBuffer( m_hdive, &m_ulSrcBufNum,
                               pkvas->fccSrcColor,
                               pkvas->szlSrcSize.cx, pkvas->szlSrcSize.cy,
                               ulStride, m_pbSrcImgBuf );

    m_brSrc.ulXOrg = 0;
    m_brSrc.ulYOrg = 0;
    m_brSrc.ulXExt = pkvas->szlSrcSize.cx;
    m_brSrc.ulYExt = pkvas->szlSrcSize.cy;

    m_sb.ulStructLen       = sizeof( SETUP_BLITTER );
    m_sb.fccSrcColorFormat = pkvas->fccSrcColor;
    m_sb.ulSrcWidth        = pkvas->rclSrcRect.xRight - pkvas->rclSrcRect.xLeft;
    m_sb.ulSrcHeight       = pkvas->rclSrcRect.yBottom - pkvas->rclSrcRect.yTop;
    m_sb.ulSrcPosX         = pkvas->rclSrcRect.xLeft;
    m_sb.ulSrcPosY         = pkvas->rclSrcRect.yTop;
    m_sb.fInvert           = pkvas->fInvert;
    m_sb.ulDitherType      = ( ULONG )pkvas->fDither;

    rc = destSetup();
    if( rc )
    {
        m_pfnDiveFreeImageBuffer( m_hdive, m_ulSrcBufNum );

        free( m_pbSrcImgBuf );

        m_ulSrcBufNum = 0;
    }

    return rc;
}

static APIRET APIENTRY vmanCaps( PKVACAPS pkvac )
{
    BYTE bRLen, bROfs, bGLen, bGOfs, bBLen, bBOfs;

    pkvac->ulMode    = KVAM_VMAN;
    pkvac->ulDepth   = m_mi.ulBpp;
    pkvac->cxScreen  = m_mi.ulHorizResolution;
    pkvac->cyScreen  = m_mi.ulVertResolution;
    pkvac->fccScreen = m_mi.fccColorEncoding;

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
            if( pkvac->fccScreen == FOURCC_BGR4 )
                pkvac->ulInputFormatFlags |= KVAF_BGR32;
            else
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

APIRET APIENTRY vmanQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    return KVAE_NO_ATTRIBUTE;
}

APIRET APIENTRY vmanSetAttr( ULONG ulAttr, PULONG pulValue )
{
    return KVAE_NO_ATTRIBUTE;
}
