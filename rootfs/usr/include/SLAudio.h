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
#define SLAUDIO_DECLSPEC __attribute__ ((visibility("default")))


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
extern SLAUDIO_DECLSPEC void SLAudio_SetLogLevel( ESLAudioLog eLogLevel );


//--------------------------------------------------------------------------------------------------
// Set the log function for the SLAudio library
// The default log function prints errors to stderr and other messages to standard output.
// Setting NULL disables log output. Log messages end with a newline.
//--------------------------------------------------------------------------------------------------
typedef void (*SLAudio_LogFunction)( void *pContext, ESLAudioLog eLogLevel, const char *pszMessage );
extern SLAUDIO_DECLSPEC void SLAudio_SetLogFunction( SLAudio_LogFunction pFunction, void *pContext );


//--------------------------------------------------------------------------------------------------
// Create a Steam Link audio context
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC CSLAudioContext *SLAudio_CreateContext();


//--------------------------------------------------------------------------------------------------
// Get the number of speakers configured on the system
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC int SLAudio_GetSpeakerCount( CSLAudioContext *pContext );


//--------------------------------------------------------------------------------------------------
// Create an audio stream
//
// The data format is 16-bit signed PCM data, and you write nFrameSizeBytes bytes at a time.
//
// The native frequency is 48000 Hz, and multi-channel audio uses the following channel order:
//
// 1 channel
// 2 channels: left, right
// 3 channels: left, center, right
// 4 channels: front left, front right, rear left, rear right
// 6 channels: front left, center, front right, rear left, rear right, LFE
//
// You should calculate nFrameSizeBytes as the amount of data you want to write in one chunk,
// for example if you're opening at 48000 Hz, 2 channels, you might want to write 50 ms at a
// time, which would be 48 * 50 * 2 * sizeof(int16_t) = 9600
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC CSLAudioStream *SLAudio_CreateStream( CSLAudioContext *pContext, int nFrequency, int nChannels, int nFrameSizeBytes, int bLowLatency );


//--------------------------------------------------------------------------------------------------
// Get a pointer to the data for the next frame
// You should write exactly nFrameSizeBytes bytes of audio data and then call SLAudio_SubmitFrame()
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC void *SLAudio_BeginFrame( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Submit the frame data for for playback
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC void SLAudio_SubmitFrame( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Return the number of audio samples (nChannels * sizeof(int16_t)) currently queued for playback
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC uint32_t SLAudio_GetQueuedAudioSamples( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free an audio stream
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC void SLAudio_FreeStream( CSLAudioStream *pStream );


//--------------------------------------------------------------------------------------------------
// Free a Steam Link audio context
//--------------------------------------------------------------------------------------------------
extern SLAUDIO_DECLSPEC void SLAudio_FreeContext( CSLAudioContext *pContext );


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
