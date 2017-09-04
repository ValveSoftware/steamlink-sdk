// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/external_clear_key/fake_cdm_video_decoder.h"

#include "base/logging.h"

namespace media {

FakeCdmVideoDecoder::FakeCdmVideoDecoder(cdm::Host* host)
    : is_initialized_(false),
      host_(host) {
}

FakeCdmVideoDecoder::~FakeCdmVideoDecoder() {
  Deinitialize();
}

bool FakeCdmVideoDecoder::Initialize(const cdm::VideoDecoderConfig& config) {
  DVLOG(1) << "Initialize()";

  video_size_ = config.coded_size;
  is_initialized_ = true;
  return true;
}

void FakeCdmVideoDecoder::Deinitialize() {
  DVLOG(1) << "Deinitialize()";
  is_initialized_ = false;
}

void FakeCdmVideoDecoder::Reset() {
  DVLOG(1) << "Reset()";
}

// Creates a YV12 video frame.
cdm::Status FakeCdmVideoDecoder::DecodeFrame(const uint8_t* compressed_frame,
                                             int32_t compressed_frame_size,
                                             int64_t timestamp,
                                             cdm::VideoFrame* decoded_frame) {
  DVLOG(1) << "DecodeFrame()";

  // The fake decoder does not buffer any frames internally. So if the input is
  // empty (EOS), just return kNeedMoreData.
  if (!decoded_frame)
    return cdm::kNeedMoreData;

  // Choose non-zero alignment and padding on purpose for testing.
  const int kAlignment = 8;
  const int kPadding = 16;
  const int kPlanePadding = 128;

  int width = video_size_.width;
  int height = video_size_.height;
  DCHECK_EQ(width % 2, 0);
  DCHECK_EQ(height % 2, 0);

  int y_stride = (width + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int uv_stride =
      (width / 2 + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int y_rows = height;
  int uv_rows = height / 2;
  int y_offset = 0;
  int v_offset = y_stride * y_rows + kPlanePadding;
  int u_offset = v_offset + uv_stride * uv_rows + kPlanePadding;
  int frame_size = u_offset + uv_stride * uv_rows + kPlanePadding;

  decoded_frame->SetFrameBuffer(host_->Allocate(frame_size));
  decoded_frame->FrameBuffer()->SetSize(frame_size);

  decoded_frame->SetFormat(cdm::kYv12);
  decoded_frame->SetSize(video_size_);
  decoded_frame->SetPlaneOffset(cdm::VideoFrame::kYPlane, y_offset);
  decoded_frame->SetPlaneOffset(cdm::VideoFrame::kVPlane, v_offset);
  decoded_frame->SetPlaneOffset(cdm::VideoFrame::kUPlane, u_offset);
  decoded_frame->SetStride(cdm::VideoFrame::kYPlane, y_stride);
  decoded_frame->SetStride(cdm::VideoFrame::kVPlane, uv_stride);
  decoded_frame->SetStride(cdm::VideoFrame::kUPlane, uv_stride);
  decoded_frame->SetTimestamp(timestamp);

  static unsigned char color = 0;
  color += 10;

  memset(reinterpret_cast<void*>(decoded_frame->FrameBuffer()->Data()),
         color, frame_size);

  return cdm::kSuccess;
}

}  // namespace media
