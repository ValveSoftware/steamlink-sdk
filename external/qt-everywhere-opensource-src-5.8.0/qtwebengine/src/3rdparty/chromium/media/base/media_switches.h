// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace switches {

MEDIA_EXPORT extern const char kAudioBufferSize[];

MEDIA_EXPORT extern const char kVideoThreads[];

MEDIA_EXPORT extern const char kEnableMediaSuspend[];
MEDIA_EXPORT extern const char kDisableMediaSuspend[];

MEDIA_EXPORT extern const char kReportVp9AsAnUnsupportedMimeType[];

#if defined(OS_ANDROID)
MEDIA_EXPORT extern const char kDisableMediaThreadForMediaPlayback[];
MEDIA_EXPORT extern const char kDisableUnifiedMediaPipeline[];
MEDIA_EXPORT extern const char kEnableMediaThreadForMediaPlayback[];
#endif

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
MEDIA_EXPORT extern const char kAlsaInputDevice[];
MEDIA_EXPORT extern const char kAlsaOutputDevice[];
#endif

MEDIA_EXPORT extern const char kUseGpuMemoryBuffersForCapture[];

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceMediaFoundationVideoCapture[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
MEDIA_EXPORT extern const char kTrySupportedChannelLayouts[];
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

#if !defined(OS_ANDROID)
MEDIA_EXPORT extern const char kEnableDefaultMediaSession[];
#endif

MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];
MEDIA_EXPORT extern const char kUseFileForFakeAudioCapture[];

MEDIA_EXPORT extern const char kEnableInbandTextTracks[];

MEDIA_EXPORT extern const char kRequireAudioHardwareForTesting[];

MEDIA_EXPORT extern const char kVideoUnderflowThresholdMs[];

MEDIA_EXPORT extern const char kDisableRTCSmoothnessAlgorithm[];

MEDIA_EXPORT extern const char kEnableVp9InMp4[];

MEDIA_EXPORT extern const char kCastEncoderUtilHeuristic[];

}  // namespace switches

namespace media {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
MEDIA_EXPORT extern const base::Feature kNewAudioRenderingMixingStrategy;
MEDIA_EXPORT extern const base::Feature kUseNewMediaCache;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
