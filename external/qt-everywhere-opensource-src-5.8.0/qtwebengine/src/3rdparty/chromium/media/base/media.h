// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code that should be used for initializing, or querying the state
// of the media library as a whole.

#ifndef MEDIA_BASE_MEDIA_H_
#define MEDIA_BASE_MEDIA_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace base {
class FilePath;
}

namespace media {

// Initializes media libraries (e.g. ffmpeg) as well as CPU specific media
// features.
MEDIA_EXPORT void InitializeMediaLibrary();

#if defined(OS_ANDROID)
// Tells the media library it has support for OS level decoders. Should only be
// used for actual decoders (e.g. MediaCodec) and not full-featured players
// (e.g. MediaPlayer).
MEDIA_EXPORT void EnablePlatformDecoderSupport();
MEDIA_EXPORT bool HasPlatformDecoderSupport();

// Indicates if the platform supports Opus. Determined *ONLY* by the platform
// version, so does not guarantee that either can actually be played.
MEDIA_EXPORT bool PlatformHasOpusSupport();

// Returns true if the unified media pipeline is enabled; the pipeline may still
// not work for all codecs if HasPlatformDecoderSupport() is false. Please see
// MimeUtil for an exhaustive listing of supported codecs.
//
// TODO(dalecurtis): These methods are temporary and should be removed once the
// unified media pipeline is supported everywhere.  http://crbug.com/580626.
MEDIA_EXPORT bool IsUnifiedMediaPipelineEnabled();

// Returns whether the platform decoders are available for use.
// This includes decoders being available on the platform and accessible, such
// as via the GPU process. Should only be used for actual decoders
// (e.g. MediaCodec) and not full-featured players (e.g. MediaPlayer).
MEDIA_EXPORT bool ArePlatformDecodersAvailable();
#endif

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_H_
