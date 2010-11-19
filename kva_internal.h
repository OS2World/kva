/*
    Internal interface for K Video Accelerator
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

#ifndef __KVA_INTERNAL_H__
#define __KVA_INTERNAL_H__

#ifdef __cplusplus
exter "C" {
#endif

#ifdef __IBMC__
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( * callconv name )arg
#else
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( callconv * name )arg
#endif

extern DECLARE_PFN( APIRET, APIENTRY, g_pfnDone, ( VOID ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnLockBuffer, ( PPVOID ppBuffer, PULONG pulBPL ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnUnlockBuffer, ( VOID ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnSetup, ( PKVASETUP pkvas ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnCaps, ( PKVACAPS pkvac ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnQueryAttr, ( ULONG ulAttr, PULONG pulValue ));
extern DECLARE_PFN( APIRET, APIENTRY, g_pfnSetAttr, ( ULONG ulAttr, PULONG pulValue ));

extern HWND  g_hwndKVA;
extern ULONG g_ulKeyColor;

#ifdef __cplusplus
}
#endif

#endif
