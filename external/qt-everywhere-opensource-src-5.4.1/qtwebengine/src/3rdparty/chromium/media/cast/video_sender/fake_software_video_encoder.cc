// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/fake_software_video_encoder.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "media/cast/transport/cast_transport_config.h"

#ifndef OFFICIAL_BUILD

namespace media {
namespace cast {

FakeSoftwareVideoEncoder::FakeSoftwareVideoEncoder(
    const VideoSenderConfig& video_config)
    : video_config_(video_config),
      next_frame_is_key_(true),
      frame_id_(0),
      frame_id_to_reference_(0),
      frame_size_(0) {
}

FakeSoftwareVideoEncoder::~FakeSoftwareVideoEncoder() {}

void FakeSoftwareVideoEncoder::Initialize() {}

bool FakeSoftwareVideoEncoder::Encode(
    const scoped_refptr<media::VideoFrame>& video_frame,
    transport::EncodedFrame* encoded_image) {
  encoded_image->frame_id = frame_id_++;
  if (next_frame_is_key_) {
    encoded_image->dependency = transport::EncodedFrame::KEY;
    encoded_image->referenced_frame_id = encoded_image->frame_id;
    next_frame_is_key_ = false;
  } else {
    encoded_image->dependency = transport::EncodedFrame::DEPENDENT;
    encoded_image->referenced_frame_id = encoded_image->frame_id - 1;
  }

  base::DictionaryValue values;
  values.SetBoolean("key",
                    encoded_image->dependency == transport::EncodedFrame::KEY);
  values.SetInteger("ref", encoded_image->referenced_frame_id);
  values.SetInteger("id", encoded_image->frame_id);
  values.SetInteger("size", frame_size_);
  base::JSONWriter::Write(&values, &encoded_image->data);
  encoded_image->data.resize(
      std::max<size_t>(encoded_image->data.size(), frame_size_));
  return true;
}

void FakeSoftwareVideoEncoder::UpdateRates(uint32 new_bitrate) {
  frame_size_ = new_bitrate / video_config_.max_frame_rate / 8;
}

void FakeSoftwareVideoEncoder::GenerateKeyFrame() {
  next_frame_is_key_ = true;
}

void FakeSoftwareVideoEncoder::LatestFrameIdToReference(uint32 frame_id) {
  frame_id_to_reference_ = frame_id;
}

}  // namespace cast
}  // namespace media

#endif
