// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media_codec_support_shlib.h"

namespace chromecast {
namespace media {

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
}

void CastMediaShlib::Finalize() {
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return nullptr;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  return nullptr;
}

MediaCodecSupportShlib::CodecSupport MediaCodecSupportShlib::IsSupported(
    const std::string& codec) {
  return kDefault;
}

double CastMediaShlib::GetMediaClockRate() {
  return 0.0;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 0.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  return false;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return false;
}

}  // namespace media
}  // namespace chromecast
