/*
    WarpOverlay interface for K Video Accelerator
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
        KO Myung-Hun <komh@chollian.net> 2007/02/07
            - Use kvaClearRect() instead of kvaClearWindow()
            - Changed kvaClearRect() called in kvaSetup()

        KO Myung-Hun <komh@chollian.net> 2007/02/20
            - Use DECLARE_PFN macro to declare function pointer for
              compatibility with IBMC mode

        KO Myung-Hun <komh@chollian.net> 2007/02/25
            - Added woQueryAttr() and woSetAttr()

        KO Myung-Hun <komh@chollian.net> 2007/02/28
            - Use WM_SIZE and WM_MOVE instead of WM_VRNENABLED and
              WM_VRNDISABLED to prevent from flickering whenever visible
              region is changed

        KO Myung-Hun <komh@chollian.net> 2007/03/01
            - Use type-cast ULONG explicitly for comparison with -1

        KO Myung-Hun <komh@chollian.net> 2007/11/25
            - Modified the codes to support the coordinate system change of
              kvaAdjustDstRect()

        KO Myung-Hun <komh@chollian.net> 2007/12/24
            - Removed 'hwnd' parameter from kvaAdjustDstRect()

        KO Myung-Hun <komh@chollian.net> 2008/01/17
            - Added support of ulInputFormatFlags
            - Added support of R565 color format
*/

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <mmioos2.h>
#include <fourcc.h>

#include <stdlib.h>
#include <string.h>

#include "hwvideo.h"

#include "kva.h"
#include "kva_internal.h"
#include "kva_wo.h"

static HMODULE      m_HWVideoHandle = NULLHANDLE;
static HWVIDEOCAPS  m_hwvc = { 0 };
static HWVIDEOSETUP m_hwvs = { 0 };

static PFNWP        m_pfnwpOld = NULL;

static ULONG        m_aulAttr[ KVAA_LAST ] = { -1, -1, -1, -1 };

// WarpOverlay! functions pointers
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOInit, ( VOID ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOCaps, ( PHWVIDEOCAPS ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOSetup, ( PHWVIDEOSETUP ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOBeginUpdate, ( PVOID, PULONG ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOEndUpdate, ( VOID ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOGetAttrib, ( ULONG, PHWATTRIBUTE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOSetAttrib, ( ULONG, PHWATTRIBUTE ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnHWVIDEOClose, ( VOID ));

static APIRET APIENTRY woDone( VOID );
static APIRET APIENTRY woLockBuffer( PPVOID ppBuffer, PULONG pulBPL );
static APIRET APIENTRY woUnlockBuffer( VOID );
static APIRET APIENTRY woSetup( PKVASETUP pkvas );
static APIRET APIENTRY woCaps( PKVACAPS pkvac );
static APIRET APIENTRY woQueryAttr( ULONG ulAttr, PULONG pulValue );
static APIRET APIENTRY woSetAttr( ULONG ulAttr, PULONG pulValue );

// Just put DLL loading into separate function
static BOOL LoadOverlay(void)
{
    char szTempStr[ 255 ];

    // Load WarpOverlay! API DLL
    if( DosLoadModule( szTempStr, sizeof( szTempStr ), "hwvideo.dll", &m_HWVideoHandle ))
        return FALSE;

    // Get all functions entry points
    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOInit", ( PFN * )&m_pfnHWVIDEOInit ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOCaps", ( PFN * )&m_pfnHWVIDEOCaps ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOSetup", ( PFN * )&m_pfnHWVIDEOSetup ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOBeginUpdate", ( PFN * )&m_pfnHWVIDEOBeginUpdate ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOEndUpdate", ( PFN * )&m_pfnHWVIDEOEndUpdate ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOGetAttrib", ( PFN * )&m_pfnHWVIDEOGetAttrib ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOSetAttrib", ( PFN * )&m_pfnHWVIDEOSetAttrib ))
        return FALSE;

    if( DosQueryProcAddr( m_HWVideoHandle, 0, "HWVIDEOClose", ( PFN * )&m_pfnHWVIDEOClose ))
        return FALSE;

    return TRUE;
}

static MRESULT EXPENTRY woNewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_ERASEBACKGROUND :
        {
            HPS     hpsFrame = ( HPS )mp1;
            PRECTL  prcl = ( PRECTL )mp2;

            GpiCreateLogColorTable( hpsFrame, 0, LCOLF_RGB, 0, 0, NULL );
            WinFillRect( hpsFrame, prcl, m_hwvs.ulKeyColor);

            return FALSE;
        }

        case WM_SIZE :
        case WM_MOVE :
            kvaAdjustDstRect( &m_hwvs.rctlSrcRect, &m_hwvs.rctlDstRect );
            WinMapWindowPoints( hwnd, HWND_DESKTOP, ( PPOINTL )&m_hwvs.rctlDstRect, 2 );
            m_pfnHWVIDEOSetup( &m_hwvs );
            break; // fall through to old window proc
    }

    return m_pfnwpOld( hwnd, msg, mp1, mp2 );
}

APIRET APIENTRY woInit( VOID )
{
    BOOL        fWOInited = FALSE;
    HWATTRIBUTE attr;
    int         i;
    CHAR        szClassName[ 80 ];
    CLASSINFO   ci;
    ULONG       rc;

    m_HWVideoHandle = NULLHANDLE;
    memset( &m_hwvc, 0, sizeof( HWVIDEOCAPS ));
    memset( &m_hwvs, 0, sizeof( HWVIDEOSETUP ));

    m_pfnwpOld = NULL;

    rc = KVAE_NO_ERROR;

    if( !LoadOverlay())
    {
        rc = KVAE_CANNOT_LOAD_WO;
        goto exit_error;
    }

    rc = m_pfnHWVIDEOInit();
    if( rc )
        goto exit_error;

    fWOInited = TRUE;

    // Query overlay capabilities
    m_hwvc.ulLength = sizeof( m_hwvc );

    // First time we need to call with zero value
    m_hwvc.ulNumColors = 0;
    m_hwvc.fccColorType = NULL;
    m_pfnHWVIDEOCaps( &m_hwvc );

    // this time hwvc.ulNumColors filled with actual count of supported FOURCCs
    // but need to check this
    if( m_hwvc.ulNumColors )
    {
        m_hwvc.fccColorType = malloc( m_hwvc.ulNumColors * sizeof( FOURCC ));
        m_pfnHWVIDEOCaps( &m_hwvc );
    }
    else
    {
        rc = KVAE_NO_SUPPORTED_FOURCC;
        goto exit_error;
    }

    if( m_hwvc.fccDstColor == FOURCC_LUT8 )
    {
        // WO doesn't supports palettized mode.
        rc = KVAE_WO_PALETTIZED_MODE;
        goto exit_error;
    }

    attr.ulLength = sizeof( HWATTRIBUTE );

    for( i = 0; i < m_hwvc.ulAttrCount; i++ )
    {
        m_pfnHWVIDEOGetAttrib( i, &attr );

        if( !strcmp( attr.szAttrDesc, ATTRIBUTE_BRIGHTNESS ))
            m_aulAttr[ KVAA_BRIGHTNESS ] = i;
        else if ( !strcmp( attr.szAttrDesc, ATTRIBUTE_CONTRAST ))
            m_aulAttr[ KVAA_CONTRAST ] = i;
        else if( !strcmp( attr.szAttrDesc, ATTRIBUTE_SATURATION ))
            m_aulAttr[ KVAA_SATURATION ] = i;
        else if( !strcmp( attr.szAttrDesc, ATTRIBUTE_HUE ))
            m_aulAttr[ KVAA_HUE ] = i;
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

    m_pfnwpOld = WinSubclassWindow( g_hwndKVA, woNewWindowProc );

    if( m_pfnwpOld )
    {
        g_pfnDone = woDone;
        g_pfnLockBuffer = woLockBuffer;
        g_pfnUnlockBuffer = woUnlockBuffer;
        g_pfnSetup = woSetup;
        g_pfnCaps = woCaps;
        g_pfnQueryAttr = woQueryAttr;
        g_pfnSetAttr = woSetAttr;

        m_hwvs.ulKeyColor = g_ulKeyColor;
    }
    else
    {
        rc = KVAE_CANNOT_SUBCLASS;
        goto exit_error;
    }

    return KVAE_NO_ERROR;

exit_error :
    if( m_HWVideoHandle )
    {
        if( m_hwvc.fccColorType )
            free( m_hwvc.fccColorType );

        if( fWOInited )
            m_pfnHWVIDEOClose();

        DosFreeModule( m_HWVideoHandle );
    }

    return rc;
}

static APIRET APIENTRY woDone( VOID )
{
    ULONG rc;

    free( m_hwvc.fccColorType );

    WinSubclassWindow( g_hwndKVA, m_pfnwpOld );

    rc = m_pfnHWVIDEOClose();

    DosFreeModule( m_HWVideoHandle );

    return rc;
}

static APIRET APIENTRY woLockBuffer( PPVOID ppBuffer, PULONG pulBPL )
{
    ULONG   ulPhysBuffer;

    *pulBPL = m_hwvs.ulSrcPitch;

    return m_pfnHWVIDEOBeginUpdate( ppBuffer, &ulPhysBuffer );
}

static APIRET APIENTRY woUnlockBuffer( VOID )
{
    return m_pfnHWVIDEOEndUpdate();
}

#ifndef min
#define min( a, b ) (( a ) < ( b ) ? ( a ) : ( b ))
#endif

#ifndef max
#define max( a, b ) (( a ) > ( b ) ? ( a ) : ( b ))
#endif

static APIRET APIENTRY woSetup( PKVASETUP pkvas )
{
    m_hwvs.ulLength = sizeof( HWVIDEOSETUP );

    m_hwvs.rctlSrcRect.xLeft = min( pkvas->rclSrcRect.xLeft, pkvas->rclSrcRect.xRight );
    m_hwvs.rctlSrcRect.xRight = max( pkvas->rclSrcRect.xLeft, pkvas->rclSrcRect.xRight );
    m_hwvs.rctlSrcRect.yTop = min( pkvas->rclSrcRect.yTop, pkvas->rclSrcRect.yBottom );
    m_hwvs.rctlSrcRect.yBottom = max( pkvas->rclSrcRect.yTop, pkvas->rclSrcRect.yBottom );

    m_hwvs.szlSrcSize = pkvas->szlSrcSize;
    m_hwvs.fccColor = pkvas->fccSrcColor;

    m_hwvs.ulSrcPitch = ( m_hwvs.szlSrcSize.cx * 2 + m_hwvc.ulScanAlign ) & ~m_hwvc.ulScanAlign;

    kvaAdjustDstRect( &m_hwvs.rctlSrcRect, &m_hwvs.rctlDstRect );
    WinMapWindowPoints( g_hwndKVA, HWND_DESKTOP, ( PPOINTL )&m_hwvs.rctlDstRect, 2 );

    if( m_hwvs.rctlDstRect.xLeft == m_hwvs.rctlDstRect.xRight  ||
        m_hwvs.rctlDstRect.yTop == m_hwvs.rctlDstRect.yBottom )
        return m_pfnHWVIDEOSetup( NULL );

    return m_pfnHWVIDEOSetup( &m_hwvs );
}

#ifndef SHOW_CAPS
static APIRET APIENTRY woCaps( PKVACAPS pkvac )
{
    int i;

    switch( m_hwvc.fccDstColor )
    {
        case FOURCC_BGR4 :
            pkvac->ulDepth = 32;
            break;

        case FOURCC_BGR3 :
            pkvac->ulDepth = 24;
            break;

        case FOURCC_R565 :
        case FOURCC_R555 :  // AS DIVE do with SNAP
            pkvac->ulDepth = 16;
            break;

        default :           // WO doesn't support palettized mode.
            return KVAE_WO_PALETTIZED_MODE;
    }

    pkvac->ulMode = KVAM_WO;
    pkvac->cxScreen = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    pkvac->cyScreen = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
    pkvac->fccScreen = m_hwvc.fccDstColor;

    pkvac->ulInputFormatFlags = 0;
    for( i = 0; i < m_hwvc.ulNumColors; i++ )
    {
        switch( m_hwvc.fccColorType[ i ])
        {
            case FOURCC_Y422 :
                pkvac->ulInputFormatFlags |= KVAF_YUY2;
                break;

            case FOURCC_R565 :
                pkvac->ulInputFormatFlags |= KVAF_BGR16;
                break;
        }
    }

    return KVAE_NO_ERROR;
}
#else
#include <stdio.h>

static APIRET APIENTRY woCaps( PKVACAPS pkvac )
{
    int i;

    printf("--- WarpOverlay Caps ---\n");
    printf("ulCapsFlag = %lx\n", m_hwvc.ulCapsFlags );
    printf("SrcMax.cx = %ld, SrcMax.cy = %d\n", m_hwvc.szlSrcMax.cx, m_hwvc.szlSrcMax.cy );
    printf("Dest Margin xLeft = %ld, yBottom = %ld, xRight = %ld, yTop = %ld\n",
           m_hwvc.rctlDstMargin.xLeft, m_hwvc.rctlDstMargin.yBottom,
           m_hwvc.rctlDstMargin.xRight, m_hwvc.rctlDstMargin.yTop );
    printf("screen FOURCC = %.4s\n", &m_hwvc.fccDstColor );
    printf("ulScanAlign = %ld\n", m_hwvc.ulScanAlign );
    printf("ulNumColors = %ld\n", m_hwvc.ulNumColors );
    for( i = 0; i < m_hwvc.ulNumColors; i++ )
        printf("%dth FOURCC = %.4s\n", i, m_hwvc.fccColorType + i );
    printf("ulAttrCount = %ld\n", m_hwvc.ulAttrCount );

    return KVAE_NO_ERROR;
}
#endif

APIRET APIENTRY woQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    HWATTRIBUTE attr;
    ULONG       rc;

    if( ulAttr > KVAA_HUE )
        return KVAE_NO_ATTRIBUTE;

    ulAttr = m_aulAttr[ ulAttr ];

    if( ulAttr == ( ULONG )-1 )
        return KVAE_NO_ATTRIBUTE;

    attr.ulLength = sizeof( HWATTRIBUTE );
    rc = m_pfnHWVIDEOGetAttrib( ulAttr, &attr );

    *pulValue = attr.ulCurrentValue;

    return rc;
}

APIRET APIENTRY woSetAttr( ULONG ulAttr, PULONG pulValue )
{
    HWATTRIBUTE attr;
    ULONG       rc;

    if( ulAttr > KVAA_HUE )
        return KVAE_NO_ATTRIBUTE;

    ulAttr = m_aulAttr[ ulAttr ];

    if( ulAttr == ( ULONG )-1 )
        return KVAE_NO_ATTRIBUTE;

    attr.ulLength = sizeof( HWATTRIBUTE );
    rc = m_pfnHWVIDEOGetAttrib( ulAttr, &attr );
    if( rc )
        return rc;

    if( *pulValue == ( ULONG )-1 )
        *pulValue = attr.ulDefaultValue;

    if(( LONG )*pulValue < 0 )
        *pulValue = 0;

    if( *pulValue > 255 )
        *pulValue = 255;

    attr.ulCurrentValue = *pulValue;

    return m_pfnHWVIDEOSetAttrib( ulAttr, &attr );
}
