/*
    Internal interface for K Video Accelerator
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

#ifndef __KVA_INTERNAL_H__
#define __KVA_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __IBMC__
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( * callconv name )arg
#else
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( callconv * name )arg
#endif

typedef struct tagKVAAPIS
{
    DECLARE_PFN( APIRET, APIENTRY, pfnDone, ( VOID ));
    DECLARE_PFN( APIRET, APIENTRY,
                 pfnLockBuffer, ( PPVOID ppBuffer, PULONG pulBPL ));
    DECLARE_PFN( APIRET, APIENTRY, pfnUnlockBuffer, ( VOID ));
    DECLARE_PFN( APIRET, APIENTRY, pfnSetup, ( PKVASETUP pkvas ));
    DECLARE_PFN( APIRET, APIENTRY, pfnCaps, ( PKVACAPS pkvac ));
    DECLARE_PFN( APIRET, APIENTRY,
                 pfnQueryAttr, ( ULONG ulAttr, PULONG pulValue ));
    DECLARE_PFN( APIRET, APIENTRY,
                 pfnSetAttr, ( ULONG ulAttr, PULONG pulValue ));
} KVAAPIS, *PKVAAPIS;

#ifdef __cplusplus
}
#endif

#endif
