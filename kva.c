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
#include "kva_vman.h"

#define KVA_SHARED_MUTEX_NAME   "\\SEM32\\KVA\\HWINUSE"
#define KVA_SHARED_MEM_NAME     "\\SHAREMEM\\KVA\\HWINUSE"

static KVAAPIS  m_kva = { NULL, };

static HWND     m_hwndKVA = NULLHANDLE;
static ULONG    m_ulKeyColor = -1;

static ULONG    m_ulRatio = KVAR_NONE;
static ULONG    m_ulAspectWidth = -1;
static ULONG    m_ulAspectHeight = -1;
static RECTL    m_rclDstRect = { 0, };

static BOOL     m_fKVAInited = FALSE;
static BOOL     m_fLocked = FALSE;

static HMTX     m_hmtxHWInUse = NULLHANDLE;
static PBOOL    m_pfHWInUse = NULL;
static BOOL     m_fHWInUse = FALSE;

static HMODULE  m_hmodSSCore = NULLHANDLE;
static PFN      m_pfnSSCore_TempDisable = NULL;
static PFN      m_pfnSSCore_TempEnable = NULL;

APIRET APIENTRY kvaInit( ULONG kvaMode, HWND hwnd, ULONG ulKeyColor )
{
    static struct tagINITROUTINE
    {
        ULONG mode;
        DECLARE_PFN( APIRET, APIENTRY, init, ( HWND, ULONG, PKVAAPIS ));
        BOOL useHW;
        const char *name;
    } initRoutines[] = {
        { KVAM_SNAP, kvaSnapInit, TRUE, "SNAP" },
        { KVAM_WO, kvaWoInit, TRUE, "WO" },
        { KVAM_VMAN, kvaVmanInit, FALSE, "VMAN" },
        { KVAM_DIVE, kvaDiveInit, FALSE, "DIVE" },
        { 0, NULL, FALSE, NULL },
    }, *initRoutine;

    const char *pszAutoMode;

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

    memset( &m_kva, 0, sizeof( m_kva ));

    m_hwndKVA = NULLHANDLE;
    m_ulKeyColor = -1;

    rc = KVAE_INVALID_PARAMETER;

    if( !hwnd )
        return rc;

    if( DosCreateMutexSem( KVA_SHARED_MUTEX_NAME, &m_hmtxHWInUse, 0, FALSE ))
    {
        if( DosOpenMutexSem( KVA_SHARED_MUTEX_NAME, &m_hmtxHWInUse ))
            return KVAE_NOT_ENOUGH_MEMORY;
    }

    DosRequestMutexSem( m_hmtxHWInUse, SEM_INDEFINITE_WAIT );

    if( DosAllocSharedMem(( PPVOID )&m_pfHWInUse, KVA_SHARED_MEM_NAME,
                          sizeof( BOOL ), fALLOC ))
    {
        if( DosGetNamedSharedMem(( PPVOID )&m_pfHWInUse, KVA_SHARED_MEM_NAME,
                                 fPERM ))
        {
            DosReleaseMutexSem( m_hmtxHWInUse );
            DosCloseMutexSem( m_hmtxHWInUse );

            return KVAE_NOT_ENOUGH_MEMORY;
        }
    }
    else
        *m_pfHWInUse = FALSE;


    m_hwndKVA = hwnd;
    m_ulKeyColor = ulKeyColor;

    // Use the specified mode by KVA_AUTOMODE if auto mode
    if( kvaMode == KVAM_AUTO &&
        ( pszAutoMode = getenv("KVA_AUTOMODE")) != NULL )
    {
        // Find corresponding mode
        for( initRoutine = initRoutines; initRoutine->name; initRoutine++ )
        {
            if( !stricmp( pszAutoMode, initRoutine->name ))
            {
                kvaMode = initRoutine->mode;
                break;
            }
        }
    }

    for( initRoutine = initRoutines; initRoutine->init; initRoutine++ )
    {
        // explicitly set ?
        if( kvaMode == initRoutine->mode )
        {
            // HW can be used only once at the same time
            if( *m_pfHWInUse && initRoutine->useHW )
            {
                DosFreeMem( m_pfHWInUse );

                DosReleaseMutexSem( m_hmtxHWInUse );
                DosClose( m_hmtxHWInUse );

                return KVAE_HW_IN_USE;
            }
        }
        // searching ?
        else if( kvaMode == KVAM_AUTO )
        {
            // do not give up. search next
            if( *m_pfHWInUse && initRoutine->useHW )
                continue;
        }
        // not wanted mode. search next
        else
            continue;

        rc = initRoutine->init( m_hwndKVA, m_ulKeyColor, &m_kva );
        if( rc )
        {
            // explicitly set ? then fail
            if( kvaMode != KVAM_AUTO )
            {
                DosFreeMem( m_pfHWInUse );

                DosReleaseMutexSem( m_hmtxHWInUse );
                DosClose( m_hmtxHWInUse );

                return rc;
            }

            // AUTO mode, search next
        }
        else
        {
            if( initRoutine->useHW )
                *m_pfHWInUse = m_fHWInUse = TRUE;

            break;
        }
    }

    DosReleaseMutexSem( m_hmtxHWInUse );

    if( !rc )
    {
        CHAR szError[ 256 ];

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
    else
    {
        DosFreeMem( m_pfHWInUse );

        DosClose( m_hmtxHWInUse );
    }

    return rc;
}

APIRET APIENTRY kvaDone( VOID )
{
    ULONG rc;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    rc = m_kva.pfnDone();
    if( rc )
        return rc;

    DosFreeModule( m_hmodSSCore );

    DosRequestMutexSem( m_hmtxHWInUse, SEM_INDEFINITE_WAIT );

    if( m_fHWInUse )
        *m_pfHWInUse = FALSE;

    DosReleaseMutexSem( m_hmtxHWInUse );

    DosFreeMem( m_pfHWInUse );

    DosCloseMutexSem( m_hmtxHWInUse );

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

    rc = m_kva.pfnLockBuffer( ppBuffer, pulBPL );
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

    rc = m_kva.pfnUnlockBuffer();
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
    m_rclDstRect = pkvas->rclDstRect;

    rc = m_kva.pfnSetup( pkvas );

    kvaClearRect( NULL );

    return rc;
}

APIRET APIENTRY kvaCaps( PKVACAPS pkvac )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pkvac )
        return KVAE_INVALID_PARAMETER;

    memset( pkvac, 0, sizeof( KVACAPS ));

    return m_kva.pfnCaps( pkvac );
}

APIRET APIENTRY kvaClearRect( PRECTL prcl )
{
    HPS     hps;
    RECTL   rcl;

    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    hps = WinGetPS( m_hwndKVA );

    if( !prcl )
    {
        prcl = &rcl;
        WinQueryWindowRect( m_hwndKVA, prcl );
    }
    GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
    WinFillRect( hps, prcl, m_ulKeyColor );

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

    if(( !prclSrc && m_ulRatio == KVAR_ORIGINAL ) || !prclDst )
        return KVAE_INVALID_PARAMETER;

    WinQueryWindowRect( m_hwndKVA, prclDst );

    if( m_rclDstRect.xLeft != m_rclDstRect.xRight &&
        m_rclDstRect.yTop != m_rclDstRect.yBottom )
    {
        // calculate a window height
        cyDst = prclDst->yTop - prclDst->yBottom;

        prclDst->xLeft = m_rclDstRect.xLeft;
        prclDst->yBottom = cyDst - m_rclDstRect.yBottom; // invert Y
        prclDst->xRight = m_rclDstRect.xRight;
        prclDst->yTop = cyDst - m_rclDstRect.yTop; // invert Y

        return KVAE_NO_ERROR;
    }

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

    WinOffsetRect( WinQueryAnchorBlock( m_hwndKVA ), prclDst,
                   ( cxDst - cxDstNew ) / 2, ( cyDst - cyDstNew ) / 2 );

    return KVAE_NO_ERROR;
}

APIRET APIENTRY kvaQueryAttr( ULONG ulAttr, PULONG pulValue )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pulValue )
        return KVAE_INVALID_PARAMETER;

    return m_kva.pfnQueryAttr( ulAttr, pulValue );
}

APIRET APIENTRY kvaSetAttr( ULONG ulAttr, PULONG pulValue )
{
    if( !m_fKVAInited )
        return KVAE_NOT_INITIALIZED;

    if( !pulValue )
        return KVAE_INVALID_PARAMETER;

    return m_kva.pfnSetAttr( ulAttr, pulValue );
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


