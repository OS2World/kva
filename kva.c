/*
    K Video Accelerator library for OS/2
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
        KO Myung-Hun <komh@chollian.net> 2007/02/06
            - Changed kvaClearWindow() to kvaClearRect()

        KO Myung-Hun <komh@chollian.net> 2007/02/07
            - Changed kvaClearRect() called in kvaSetup()

        KO Myung-Hun <komh@chollian.net> 2007/02/20
            - Use DECLARE_PFN macro to declare function pointer for
              compatibility with IBMC mode

        KO Myung-Hun <komh@chollian.net> 2007/02/25
            - Added initialization check in kvaClearRect()
            - Check return value of g_pfnDone() in kvaDone()
            - Added kvaQueryAttr(), kvaSetAttr() and kvaResetAttr()

        KO Myung-Hun <komh@chollian.net> 2007/09/29
            - Added support of KVAR_FORCEANY

        KO Myung-Hun <komh@chollian.net> 2007/11/25
            - Changed the coordinate system of 'prclDst' of kvaAdjustDstRect()
              from screen one to window one.

        KO Myung-Hun <komh@chollian.net> 2007/12/22
            - Added support of SNAP

        KO Myung-Hun <komh@chollian.net> 2007/12/24
            - Removed 'hwnd' parameter from kvaAdjustDstRect()
            - Check parameters of kvaAdjustDstRect(), more strictly

        KO Myung-Hun <komh@chollian.net> 2008/01/17
            - Changed the detection order from wo > snap > dive to
              snap > wo > dive

        KO Myung-Hun <komh@chollian.net> 2008/02/03
            - Allow only one HW overlay on system-wide

        KO Myung-Hun <komh@chollian.net> 2008/08/11
            - Load DIVE.DLL dynamically, so no need to link with mmpm2
*/

#define INCL_WIN
#define INCL_GPI
#define INCL_DOS
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#include "kva.h"
#include "kva_internal.h"
#include "kva_dive.h"
#include "kva_wo.h"
#include "kva_snap.h"

#define KVA_SHARED_MEM_NAME "\\SHAREMEM\\KVA\\HWINUSE"

DECLARE_PFN( APIRET, APIENTRY, g_pfnDone, ( VOID ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnLockBuffer, ( PPVOID ppBuffer, PULONG pulBPL ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnUnlockBuffer, ( VOID ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnSetup, ( PKVASETUP pkvas ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnCaps, ( PKVACAPS pkvac ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnQueryAttr, ( ULONG ulAttr, PULONG pulValue ));
DECLARE_PFN( APIRET, APIENTRY, g_pfnSetAttr, ( ULONG ulAttr, PULONG pulValue ));

HWND  g_hwndKVA = NULLHANDLE;
ULONG g_ulKeyColor = -1;

static ULONG    m_ulRatio = KVAR_NONE;
static ULONG    m_ulAspectWidth = -1;
static ULONG    m_ulAspectHeight = -1;

static BOOL     m_fKVAInited = FALSE;
static BOOL     m_fLocked = FALSE;

static PBOOL    m_pfHWInUse = NULL;
static BOOL     m_fHWInUse = FALSE;

static HMODULE  m_hmodSSCore = NULLHANDLE;
static PFN      m_pfnSSCore_TempDisable = NULL;
static PFN      m_pfnSSCore_TempEnable = NULL;

APIRET APIENTRY kvaInit( ULONG kvaMode, HWND hwnd, ULONG ulKeyColor )
{
    ULONG   rc;

    if( m_fKVAInited )
        return KVAE_ALREADY_INITIALIZED;

    m_fKVAInited = FALSE;
    m_fLocked = FALSE;
    m_fHWInUse = FALSE;

    m_ulRatio = KVAR_NONE;
    m_ulAspectWidth = -1;
    m_ulAspectHeight = -1;

    m_hmodSSCore = NULLHANDLE;
    m_pfnSSCore_TempDisable = NULL;
    m_pfnSSCore_TempEnable = NULL;

    g_pfnDone = NULL;
    g_pfnLockBuffer = NULL;
    g_pfnUnlockBuffer = NULL;
    g_pfnSetup = NULL;

    g_hwndKVA = NULLHANDLE;
    g_ulKeyColor = -1;

    rc = KVAE_INVALID_PARAMETER;

    if( !hwnd )
        return rc;

    if( DosGetNamedSharedMem( &m_pfHWInUse, KVA_SHARED_MEM_NAME, fPERM ) == 0 )
    {
        if( *m_pfHWInUse )
        {
            if( kvaMode == KVAM_SNAP || kvaMode == KVAM_WO )
            {
                DosFreeMem( m_pfHWInUse );

                return KVAE_HW_IN_USE;
            }
            else if( kvaMode == KVAM_AUTO )
                kvaMode = KVAM_DIVE;
        }
    }
    else
    {
        if( DosAllocSharedMem( &m_pfHWInUse, KVA_SHARED_MEM_NAME, sizeof( BOOL ), fALLOC ) != 0 )
            return KVAE_NOT_ENOUGH_MEMORY;

        *m_pfHWInUse = FALSE;
    }

    g_hwndKVA = hwnd;
    g_ulKeyColor = ulKeyColor;

    if( kvaMode == KVAM_SNAP || kvaMode == KVAM_AUTO )
    {
        rc = snapInit();
        if( rc )
        {
            if( kvaMode != KVAM_AUTO )
                return rc;
        }
        else
            kvaMode = KVAM_SNAP;
    }

    if( kvaMode == KVAM_WO || kvaMode == KVAM_AUTO )
    {
        rc = woInit();
        if( rc )
        {
            if( kvaMode != KVAM_AUTO )
                return rc;
        }
        else
            kvaMode = KVAM_WO;
    }

    if( kvaMode == KVAM_DIVE || kvaMode == KVAM_AUTO )
    {
        rc = diveInit();
        if( rc )
        {
            if( kvaMode != KVAM_AUTO )
                return rc;
        }
        else
            kvaMode = KVAM_DIVE;
    }

    if( !rc )
    {
        UCHAR szError[ 256 ];

        if( kvaMode == KVAM_SNAP || kvaMode == KVAM_WO )
            *m_pfHWInUse = m_fHWInUse = TRUE;

        if( DosLoadModule( szError, sizeof( szError ), "SSCORE", &m_hmodSSCore ) == 0 )
        {
            DosQueryProcAddr( m_hmodSSCore, 0, "SSCore_TempDisable", &m_pfnSSCore_TempDisable );
            DosQueryProcAddr( m_hmodSSCore, 0, "SSCore_TempEnable", &m_pfnSSCore_TempEnable );

            if( !m_pfnSSCore_TempDisable || !m_pfnSSCore_TempEnable )
            {
                DosFreeModule( m_hmodSSCore );

                m_hmodSSCore = NULLHANDLE;
                m_pfnSSCore_TempDisable = NULL;
                m_pfnSSCore_TempEnable = NULL;
            }
        }

        m_fKVAInited = TRUE;
    }

    return rc;
}

APIRET APIENTRY kvaDone( VOID )
{
    ULONG rc;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    rc = g_pfnDone();
    if( rc )
        return rc;

    DosFreeModule( m_hmodSSCore );

    if( m_fHWInUse )
        *m_pfHWInUse = FALSE;

    DosFreeMem( m_pfHWInUse );

    m_fKVAInited = FALSE;

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaLockBuffer( PPVOID ppBuffer, PULONG pulBPL )
{
    ULONG   rc;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( m_fLocked )
        return KVAE_ALREADY_LOCKED;

    if( ppBuffer == NULL || pulBPL == NULL )
        return KVAE_INVALID_PARAMETER;

    rc = g_pfnLockBuffer( ppBuffer, pulBPL );
    if( rc )
        return rc;

    m_fLocked = TRUE;

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaUnlockBuffer( VOID )
{
    ULONG   rc;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !m_fLocked )
        return KVAE_NOT_LOCKED;

    rc = g_pfnUnlockBuffer();
    if( rc )
        return rc;

    m_fLocked = FALSE;

    return 0;
}

APIRET APIENTRY kvaSetup( PKVASETUP pkvas )
{
    ULONG rc;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pkvas )
        return KVAE_INVALID_PARAMETER;

    m_ulRatio = pkvas->ulRatio;
    m_ulAspectWidth = pkvas->ulAspectWidth;
    m_ulAspectHeight = pkvas->ulAspectHeight;

    rc = g_pfnSetup( pkvas );

    kvaClearRect( NULL );

    return rc;
}

APIRET APIENTRY kvaCaps( PKVACAPS pkvac )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pkvac )
        return KVAE_INVALID_PARAMETER;

    memset( pkvac, 0, sizeof( PKVACAPS ));

    return g_pfnCaps( pkvac );
}

APIRET APIENTRY kvaClearRect( PRECTL prcl )
{
    HPS     hps;
    RECTL   rcl;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    hps = WinGetPS( g_hwndKVA );

    if( !prcl )
    {
        prcl = &rcl;
        WinQueryWindowRect( g_hwndKVA, prcl );
    }
    GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
    WinFillRect( hps, prcl, g_ulKeyColor );

    WinReleasePS( hps );

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaAdjustDstRect( PRECTL prclSrc, PRECTL prclDst )
{
    ULONG   cxSrc, cySrc;
    ULONG   cxDst, cyDst;
    ULONG   cxDstNew, cyDstNew;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !prclSrc || !prclDst )
        return KVAE_INVALID_PARAMETER;

    WinQueryWindowRect( g_hwndKVA, prclDst );

    cxDst = prclDst->xRight;
    cyDst = prclDst->yTop;

    switch( m_ulRatio )
    {

        case KVAR_ORIGINAL :
        case KVAR_FORCE43  :
        case KVAR_FORCE169 :
        case KVAR_FORCEANY :
            switch( m_ulRatio )
            {
                case KVAR_ORIGINAL :
                    cxSrc = labs( prclSrc->xRight - prclSrc->xLeft );
                    cySrc = labs( prclSrc->yTop - prclSrc->yBottom );
                    break;

                case KVAR_FORCE43 :
                    cxSrc = 4;
                    cySrc = 3;
                    break;

                case KVAR_FORCE169 :
                    cxSrc = 16;
                    cySrc = 9;
                    break;

                case KVAR_FORCEANY :
                default :               // to prevent from uninitializing warning
                    cxSrc = m_ulAspectWidth;
                    cySrc = m_ulAspectHeight;
                    break;
            }

            cxDstNew = cxDst;
            cyDstNew = cxDstNew * cySrc / cxSrc;
            if( cyDstNew > cyDst )
            {
                cyDstNew = cyDst;
                cxDstNew = cyDstNew * cxSrc / cySrc;
            }
            break;

        case KVAR_NONE :
        default :   // assume KVAR_NONE
            cxDstNew = cxDst;
            cyDstNew = cyDst;
            break;
    }

    prclDst->xRight = cxDstNew;
    prclDst->yTop = cyDstNew;

    WinOffsetRect( WinQueryAnchorBlock( g_hwndKVA ), prclDst,
                   ( cxDst - cxDstNew ) / 2, ( cyDst - cyDstNew ) / 2 );

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pulValue )
        return KVAE_INVALID_PARAMETER;

    return g_pfnQueryAttr( ulAttr, pulValue );
}

APIRET APIENTRY kvaSetAttr( ULONG ulAttr, PULONG pulValue )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pulValue )
        return KVAE_INVALID_PARAMETER;

    return g_pfnSetAttr( ulAttr, pulValue );
}

APIRET APIENTRY kvaResetAttr( VOID )
{
    ULONG ulValue;
    int   i;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    for( i = 0; i < KVAA_LAST; i++ )
    {
        ulValue = -1;
        kvaSetAttr( i, &ulValue );
    }

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaDisableScreenSaver( VOID )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( m_pfnSSCore_TempDisable )
        m_pfnSSCore_TempDisable();

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaEnableScreenSaver( VOID )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( m_pfnSSCore_TempEnable )
        m_pfnSSCore_TempEnable();

    return KVAE_NO_ERROR;
}


