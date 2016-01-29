/*
  SLAudio - Low latency audio support for the Steam Link

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
#ifndef _SLAudio_h
#define _SLAudio_h

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
	k_ESLAudioLogDebug,
	k_ESLAudioLogInfo,
	k_ESLAudioLogWarning,
	k_ESLAudioLogError,
} ESLAudioLog;


//--------------------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------------------
typedef struct CSLAudioContext CSLAudioContext;
typedef struct CSLAudioStream CSLAudioStream;


//--------------------------------------------------------------------------------------------------
// Set the log level for the SLAudio library
// The default log level is k_ESLAudioLogError
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLAudio_SetLogLevel( ESLAudioLog eLogLevel );


//--------------------------------------------------------------------------------------------------
// Set the log function for the SLAudio library
// The default log function prints errors to stderr and other messages to standard output.
// Setting NULL disables log output. Log messages end with a newline.
//--------------------------------------------------------------------------------------------------
typedef void (*SLAudio_LogFunction)( void *pContext, ESLAudioLog eLogLevel, const char *pszMessage );
extern SLGAMEPAD_DECLSPEC void SLAudio_SetLogFunction( SLAudio_LogFunction pFunction, void *pContext );


//--------------------------------------------------------------------------------------------------
// Create a Steam Link audio context
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC CSLAudioContext *SLAudio_CreateContext();


//--------------------------------------------------------------------------------------------------
// Get the number of speakers configured on the system
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC int SLAudio_GetSpeakerCount( CSLAudioContext *pContext );


//--------------------------------------------------------------------------------------------------
// Create an audio stream
// The data format is 16-bit signed PCM data, and you write nFrameSizeBytes bytes at a time.
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC CSLAudioStream *SLAudio_CreateStream( CSLAudioContext *pContext, int nFrequency, int nChannels, int nFrameSizeBytes );


//--------------------------------------------------------------------------------------------------
// Get a pointer to the data for the next frame
// You should write exactly nFrameSizeBytes bytes of audio data and then call SLAudio_SubmitFrame()
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void *SLAudio_BeginFrame( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Submit the frame data for for playback
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLAudio_SubmitFrame( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Return the number of audio samples currently queued for playback
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC uint32_t SLAudio_GetQueuedAudioSamples( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free an audio stream
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLAudio_FreeStream( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free a Steam Link audio context
//--------------------------------------------------------------------------------------------------
extern SLGAMEPAD_DECLSPEC void SLAudio_FreeContext( CSLAudioContext *pContext );


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

#endif // _SLAudio_h
