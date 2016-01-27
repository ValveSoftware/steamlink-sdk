/*
  SLVideo - Hardware accelerated video support for the Steam Link

  Copyright (C) 2016 Valve Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#ifndef _SLVideo_h
#define _SLVideo_h

#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Set up for C function definitions, even when using C++
//--------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif


//--------------------------------------------------------------------------------------------------
// Make sure structures are packed consistently
//--------------------------------------------------------------------------------------------------
#pragma pack(push,4)


//--------------------------------------------------------------------------------------------------
// Make sure functions are exported from the shared library
//--------------------------------------------------------------------------------------------------
#define SLGAMEPAD_DECLSPEC __attribute__ ((visibility("default")))


//--------------------------------------------------------------------------------------------------
// Enumeration of log message severity
//--------------------------------------------------------------------------------------------------
typedef enum
{
	k_ESLVideoLogDebug,
	k_ESLVideoLogInfo,
	k_ESLVideoLogWarning,
	k_ESLVideoLogError,
} ESLVideoLog;


//--------------------------------------------------------------------------------------------------
// Enumeration of available video stream types
//--------------------------------------------------------------------------------------------------
typedef enum
{
	k_ESLVideoFormatH264,

} ESLVideoFormat;


//--------------------------------------------------------------------------------------------------
// Enumeration of YUV conversion matrices
//--------------------------------------------------------------------------------------------------
typedef enum
{
	k_ESLVideoTransferMatrix_Automatic,	// BT.601 for SD content, BT.709 for HD content
	k_ESLVideoTransferMatrix_BT601,
	k_ESLVideoTransferMatrix_BT709,

} ESLVideoTransferMatrix;


//--------------------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------------------
typedef struct CSLVideoContext CSLVideoContext;
typedef struct CSLVideoOverlay CSLVideoOverlay;
typedef struct CSLVideoStream CSLVideoStream;


//--------------------------------------------------------------------------------------------------
// Set the log level for the SLVideo library
// The default log level is k_ESLVideoLogError
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetLogLevel( ESLVideoLog eLogLevel );


//--------------------------------------------------------------------------------------------------
// Set the log function for the SLVideo library
// The default log function prints errors to stderr and other messages to standard output.
// Setting NULL disables log output. Log messages end with a newline.
//--------------------------------------------------------------------------------------------------
typedef void (*SLVideo_LogFunction)( void *pContext, ESLVideoLog eLogLevel, const char *pszMessage );
extern SLGAMEPAD_DECLSPEC void SLVideo_SetLogFunction( SLVideo_LogFunction pFunction, void *pContext );


//--------------------------------------------------------------------------------------------------
// Create a Steam Link video context
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC CSLVideoContext *SLVideo_CreateContext();


//--------------------------------------------------------------------------------------------------
// Get the display resolution
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_GetDisplayResolution( CSLVideoContext *pContext, int *pnWidth, int *pnHeight );


//--------------------------------------------------------------------------------------------------
// Enable protected content (HDCP)
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_EnableProtectedContent( CSLVideoContext *pContext );


//--------------------------------------------------------------------------------------------------
// Disable protected content (HDCP)
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_DisableProtectedContent( CSLVideoContext *pContext );


//--------------------------------------------------------------------------------------------------
// Create an ARGB overlay surface
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC CSLVideoOverlay *SLVideo_CreateOverlay( CSLVideoContext *pContext, int nWidth, int nHeight );


//--------------------------------------------------------------------------------------------------
// Get the overlay size
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_GetOverlaySize( CSLVideoOverlay *pOverlay, int *pnWidth, int *pnHeight );


//--------------------------------------------------------------------------------------------------
// Get the overlay pixels
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_GetOverlayPixels( CSLVideoOverlay *pOverlay, uint32_t **ppPixels, int *pnPitch );


//--------------------------------------------------------------------------------------------------
// Set the output area for the overlay to be centered, fullscreen, with correct aspect ratio
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetOverlayDisplayFullscreen( CSLVideoOverlay *pOverlay );


//--------------------------------------------------------------------------------------------------
// Set the output area for the overlay
// The offset and size coordinates are float 0.0 - 1.0
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetOverlayDisplayArea( CSLVideoOverlay *pOverlay, float flOffsetX, float flOffsetY, float flWidth, float flHeight );


//--------------------------------------------------------------------------------------------------
// Make an overlay visible
// Only one overlay can be visible at a time. You can have double-buffered graphics by having
// two overlays and sequentially showing them to alternate which is visible on screen.
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_ShowOverlay( CSLVideoOverlay *pOverlay );


//--------------------------------------------------------------------------------------------------
// Make an overlay no longer visible
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_HideOverlay( CSLVideoOverlay *pOverlay );


//--------------------------------------------------------------------------------------------------
// Free an overlay
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_FreeOverlay( CSLVideoOverlay *pOverlay );


//--------------------------------------------------------------------------------------------------
// Create a video stream
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC CSLVideoStream *SLVideo_CreateStream( CSLVideoContext *pContext, ESLVideoFormat eFormat );


//--------------------------------------------------------------------------------------------------
// Set the video transfer matrix (optional, defaults to k_ESLVideoTransferMatrix_Automatic)
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetStreamVideoTransferMatrix( CSLVideoStream *pStream, ESLVideoTransferMatrix eVideoTransferMatrix );


//--------------------------------------------------------------------------------------------------
// Set the expected video framerate (optional)
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetStreamTargetFramerate( CSLVideoStream *pStream, unsigned unFramerateNumerator, unsigned unFramerateDenominator );


//--------------------------------------------------------------------------------------------------
// Set the expected video bitrate, in kBit/s (optional)
// The Steam Link video decoder has an approximate bit rate limit of about 30000 (30 Mbit/s)
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_SetStreamTargetBitrate( CSLVideoStream *pStream, int nBitrate );


//--------------------------------------------------------------------------------------------------
// Get the current video stream resolution or 0x0 if not yet detected
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_GetStreamResolution( CSLVideoStream *pStream, int *pnWidth, int *pnHeight );


//--------------------------------------------------------------------------------------------------
// Start decoding a frame of the given size
// This function returns 0 on success or -1 if there is a macroblock error or the decoder can't
// keep up with the data rate.
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC int SLVideo_BeginFrame( CSLVideoStream *pStream, int nFrameSize );


//--------------------------------------------------------------------------------------------------
// Add video data to the frame
// This function returns 0 on success or -1 if you try to write more data than expected
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC int SLVideo_WriteFrameData( CSLVideoStream *pStream, void *pData, int nDataSize );


//--------------------------------------------------------------------------------------------------
// Submit the frame data for decoding.
// This function returns 0 on success or -1 if there is an error submitting the frame.
// The frame will be automatically displayed when decoding is complete.
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC int SLVideo_SubmitFrame( CSLVideoStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free a video stream
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_FreeStream( CSLVideoStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free a Steam Link video context
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLVideo_FreeContext( CSLVideoContext *pContext );


//--------------------------------------------------------------------------------------------------
// Reset structure packing
//--------------------------------------------------------------------------------------------------
#pragma pack(pop)


//--------------------------------------------------------------------------------------------------
// Ends C function definitions when using C++
//--------------------------------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

#endif // _SLVideo_h
