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

/** @file kva.h
 */
#ifndef __KVA_H__
#define __KVA_H__

#include <os2.h>
#include <mmioos2.h>

#ifdef __cplusplus
extern "C" {
#endif

/** KVA version macro */
#define KVA_VERSION "1.2.2"

/**
 * @defgroup kvamodes KVA modes
 * @{
 */
#define KVAM_AUTO   0
#define KVAM_DIVE   1
#define KVAM_WO     2
#define KVAM_SNAP   3
#define KVAM_VMAN   4
/** @} */

/**
 * @defgroup kvaratios KVA ratios
 * @{
 */
#define KVAR_NONE       0
#define KVAR_ORIGINAL   1
#define KVAR_FORCE43    2
#define KVAR_FORCE169   3
#define KVAR_FORCEANY   4
/** @} */

/**
 * @defgroup kvaerros KVA error codes
 * @{
 */
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
/** @} */

/**
 * @defgroup kvaattributes KVA attributes
 * @{
 */
#define KVAA_BRIGHTNESS 0
#define KVAA_CONTRAST   1
#define KVAA_SATURATION 2
#define KVAA_HUE        3
#define KVAA_GAMMA      4
#define KVAA_LAST       ( KVAA_GAMMA + 1 )
/** @} */

/**
 * @defgroup kvaformats KVA image formats
 * @{
 */
#define KVAF_YUY2       0x00000001
#define KVAF_YV12       0x00000002
#define KVAF_YVU9       0x00000004
#define KVAF_BGR24      0x00010000
#define KVAF_BGR16      0x00020000
#define KVAF_BGR15      0x00040000
#define KVAF_BGR32      0x00080000
/** @} */

#ifndef FOURCC_YV12
/** FOURCC for YV12 */
#define FOURCC_YV12     mmioFOURCC( 'Y', 'V', '1', '2' )
#endif

#ifndef FOURCC_YVU9
/** FOURCC for YVU9 */
#define FOURCC_YVU9     mmioFOURCC( 'Y', 'V', 'U', '9' )
#endif

#pragma pack( 1 )

/**
 * @brief Setup data for #kvaSetup()
 */
typedef struct tagKVASETUP
{
    ULONG       ulLength;       /**< Size of structure in bytes */
    RECTL       rclSrcRect;     /**< Displayed subrectangle.
                                 *   Top-left is (0,0) */
    SIZEL       szlSrcSize;     /**< Source image size */
    ULONG       ulRatio;        /**< Aspect ratio */
    ULONG       ulAspectWidth;  /**< Aspect width, only for KVAR_FORCEANY */
    ULONG       ulAspectHeight; /**< Aspect height, only for KVAR_FORCEANY */
    ULONG       fccSrcColor;    /**< Image format in FOURCC */
    BOOL        fInvert;        /**< Image invert, only for DIVE */
    BOOL        fDither;        /**< Image dither, only for DIVE */
    RECTL       rclDstRect;     /**< Destination rectangle. Top-left is (0,0).
                                 *   If empty, this is calculated automatically
                                 */
} KVASETUP, *PKVASETUP;

/**
 * @brief Capability data filled by #kvaCaps()
 */
typedef struct tagKVACAPS
{
    ULONG   ulMode;             /**< KVA mode. See @ref kvamodes */
    ULONG   ulDepth;            /**< Bits depth of screen */
    ULONG   cxScreen;           /**< Width of screen in pixels */
    ULONG   cyScreen;           /**< Height of screen in pixels */
    FOURCC  fccScreen;          /**< Screen FOURCC */
    ULONG   ulInputFormatFlags; /**< Input format flags supported by current
                                 *   mode. See @ref kvaformats */
    ULONG   ulRMask;            /**< Red mask */
    ULONG   ulGMask;            /**< Green mask */
    ULONG   ulBMask;            /**< Blue mask */
} KVACAPS, *PKVACAPS;

#pragma pack()

/**
 * @brief Intialize KVA
 * @param[in] kvaMode KVA mode to use
 * @param[in] hwnd Handle of the window to display an image
 * @param[in] ulKeyColor Color key
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaInit( ULONG kvaMode, HWND hwnd, ULONG ulKeyColor );

/**
 * @brief Terminate KVA
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaDone( VOID );

/**
 * @brief Lock buffer to be filled with image datum
 * @param[out] ppBuffer Place to store an image buffer adress
 * @param[out] pulBPL Place to store BYTES per LINE, aka stride.
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaLockBuffer( PPVOID ppBuffer, PULONG pulBPL );

/**
 * @brief Unlock buffer and display an image
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaUnlockBuffer( VOID );

/**
 * @brief Setup KVA
 * @param[in] pkvas Setup data. See struct #KVASETUP
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaSetup( PKVASETUP pkvas );

/**
 * @brief Query capabilities
 * @param[out] pkvac Place to store capabilities. See struct #KVACAPS
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaCaps( PKVACAPS pkvac );

/**
 * @brief Clear given rectangle area
 * @param[in] prcl Rectangle area to be clear
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaClearRect( PRECTL prcl );

/**
 * @brief Query destination area
 * @param[in] prclSrc Source area
 * @param[out] prclDst Destination area. This is calculated automatically
 *             if KVASETUP.rclDstRect is empty
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaAdjustDstRect( PRECTL prclSrc, PRECTL prclDst );

/**
 * @brief Query attributes
 * @param[in] ulAttr Attributes
 * @param[out] pulValue Place to store an attribute value
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaQueryAttr( ULONG ulAttr, PULONG pulValue );

/**
 * @brief Set attributes
 * @param[in] ulAttr Attributes
 * @param[in/out] pulValue Attribute value. If -1, attribute is reset to
 *                default. A value out of [0, 255] is adjusted. Store the
 *                result to the address pointed by @a pulValue
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaSetAttr( ULONG ulAttr, PULONG pulValue );

/**
 * @brief Reset attributes
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaResetAttr( VOID );

/**
 * @brief Disable Doodle's screen saver
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaDisableScreenSaver( VOID );

/**
 * @brief Enable Doodle's screen saver
 * @return KVAE_NO_ERROR on success, or other error codes.
 */
APIRET APIENTRY kvaEnableScreenSaver( VOID );

#ifdef __cplusplus
}
#endif

#endif
