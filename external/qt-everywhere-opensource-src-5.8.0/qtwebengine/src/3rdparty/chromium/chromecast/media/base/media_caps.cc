// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_caps.h"

namespace media {

namespace {
int g_hdmi_codecs = 0;
}  // namespace

void SetHdmiSinkCodecs(int codecs_mask) {
  g_hdmi_codecs = codecs_mask;
}

bool HdmiSinkSupportsAC3() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecAc3;
}

bool HdmiSinkSupportsDTS() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecDts;
}

bool HdmiSinkSupportsDTSHD() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecDtsHd;
}

bool HdmiSinkSupportsEAC3() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecEac3;
}

bool HdmiSinkSupportsPcmSurroundSound() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecPcmSurroundSound;
}

}  // namespace media

