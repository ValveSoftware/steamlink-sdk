// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace switches {

MEDIA_EXPORT extern const char kAudioBufferSize[];

MEDIA_EXPORT extern const char kVideoThreads[];

MEDIA_EXPORT extern const char
    kIgnoreResolutionLimitsForAcceleratedVideoDecode[];

#if defined(OS_ANDROID)
MEDIA_EXPORT extern const char kDisableInfobarForProtectedMediaIdentifier[];
MEDIA_EXPORT extern const char kMediaDrmEnableNonCompositing[];
#endif

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
MEDIA_EXPORT extern const char kAlsaInputDevice[];
MEDIA_EXPORT extern const char kAlsaOutputDevice[];
#endif

#if defined(OS_MACOSX)
MEDIA_EXPORT extern const char kEnableAVFoundation[];
MEDIA_EXPORT extern const char kForceQTKit[];
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceDirectShowVideoCapture[];
MEDIA_EXPORT extern const char kForceMediaFoundationVideoCapture[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
MEDIA_EXPORT extern const char kTrySupportedChannelLayouts[];
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];

}  // namespace switches

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
