// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_CAPS_
#define CHROMECAST_MEDIA_BASE_MEDIA_CAPS_

namespace media {

enum HdmiSinkCodec {
  kSinkCodecAc3 = 1,
  kSinkCodecDts = 1 << 1,
  kSinkCodecDtsHd = 1 << 2,
  kSinkCodecEac3 = 1 << 3,
  kSinkCodecPcmSurroundSound = 1 << 4,
};

// Records the known supported codecs for the current HDMI sink, as a bit mask
// of HdmiSinkCodec values.
void SetHdmiSinkCodecs(int codecs_mask);

bool HdmiSinkSupportsAC3();
bool HdmiSinkSupportsDTS();
bool HdmiSinkSupportsDTSHD();
bool HdmiSinkSupportsEAC3();
bool HdmiSinkSupportsPcmSurroundSound();

}  // namespace media

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_CAPS_
