// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemotePlaybackAvailability_h
#define WebRemotePlaybackAvailability_h

namespace blink {

// GENERATED_JAVA_ENUM_PACKAGE: (
//     org.chromium.blink_public.platform.modules.remoteplayback)
// Various states for the remote playback availability.
enum class WebRemotePlaybackAvailability {
  // The availability is unknown.
  Unknown,

  // The media source is not supported by the browser - device availability
  // monitoring is unnecessary.
  SourceNotSupported,

  // The media source is compatible with some supported device types but
  // no devices were found.
  DeviceNotAvailable,

  // There're available devices but the current media source is not compatible
  // with any of those.
  SourceNotCompatible,

  // There're available remote playback devices and the media source is
  // compatible with at least one of them.
  DeviceAvailable,

  Last = DeviceAvailable
};

}  // namespace blink

#endif  // WebRemotePlaybackState_h
