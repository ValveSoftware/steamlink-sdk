// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/public/media_codec_support_shlib.h"

namespace chromecast {
namespace media {
namespace {

bool IsCodecSupported(const std::string& codec) {
  // FLAC and Opus are supported via the default renderer if the CMA backend
  // does not have explicit support.
  if (codec == "opus" || codec == "flac")
    return true;

  MediaCodecSupportShlib::CodecSupport platform_support =
      MediaCodecSupportShlib::IsSupported(codec);
  if (platform_support == MediaCodecSupportShlib::kSupported)
    return true;
  else if (platform_support == MediaCodecSupportShlib::kNotSupported)
    return false;

  if (codec == "aac51") {
    return ::media::HdmiSinkSupportsPcmSurroundSound();
  }
  if (codec == "ac-3" || codec == "mp4a.a5" || codec == "mp4a.A5") {
    return ::media::HdmiSinkSupportsAC3();
  }
  if (codec == "ec-3" || codec == "mp4a.a6" || codec == "mp4a.A6") {
    return ::media::HdmiSinkSupportsEAC3();
  }

  // This function is invoked from MimeUtil::AreSupportedCodecs to check if a
  // given codec id is supported by Chromecast or not. So by default we should
  // return true by default to indicate we have no reasons to believe this codec
  // is unsupported. This will allow the rest of MimeUtil checks to proceed as
  // usual.
  return true;
}

}  // namespace

::media::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&IsCodecSupported);
}

}  // namespace media
}  // namespace chromecast
