// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp8_decoder.h"
#include "media/base/limits.h"

namespace media {

VP8Decoder::VP8Accelerator::VP8Accelerator() {}

VP8Decoder::VP8Accelerator::~VP8Accelerator() {}

VP8Decoder::VP8Decoder(VP8Accelerator* accelerator)
    : state_(kNeedStreamMetadata),
      curr_frame_start_(nullptr),
      frame_size_(0),
      accelerator_(accelerator) {
  DCHECK(accelerator_);
}

VP8Decoder::~VP8Decoder() {}

bool VP8Decoder::Flush() {
  DVLOG(2) << "Decoder flush";
  Reset();
  return true;
}

void VP8Decoder::SetStream(const uint8_t* ptr, size_t size) {
  DCHECK(ptr);
  DCHECK(size);

  curr_frame_start_ = ptr;
  frame_size_ = size;
  DVLOG(4) << "New input stream at: " << (void*)ptr << " size: " << size;
}

void VP8Decoder::Reset() {
  curr_pic_ = nullptr;
  curr_frame_hdr_ = nullptr;
  curr_frame_start_ = nullptr;
  frame_size_ = 0;

  last_frame_ = nullptr;
  golden_frame_ = nullptr;
  alt_frame_ = nullptr;

  if (state_ == kDecoding)
    state_ = kAfterReset;
}

VP8Decoder::DecodeResult VP8Decoder::Decode() {
  if (!curr_frame_start_ || frame_size_ == 0)
    return kRanOutOfStreamData;

  if (!curr_frame_hdr_) {
    curr_frame_hdr_.reset(new Vp8FrameHeader());
    if (!parser_.ParseFrame(curr_frame_start_, frame_size_,
                            curr_frame_hdr_.get())) {
      DVLOG(1) << "Error during decode";
      state_ = kError;
      return kDecodeError;
    }
  }

  if (curr_frame_hdr_->IsKeyframe()) {
    gfx::Size new_pic_size(curr_frame_hdr_->width, curr_frame_hdr_->height);
    if (new_pic_size.IsEmpty())
      return kDecodeError;

    if (new_pic_size != pic_size_) {
      DVLOG(2) << "New resolution: " << new_pic_size.ToString();
      pic_size_ = new_pic_size;

      DCHECK(!curr_pic_);
      last_frame_ = nullptr;
      golden_frame_ = nullptr;
      alt_frame_ = nullptr;

      return kAllocateNewSurfaces;
    }

    state_ = kDecoding;
  } else {
    if (state_ != kDecoding) {
      // Need a resume point.
      curr_frame_hdr_.reset();
      return kRanOutOfStreamData;
    }
  }

  curr_pic_ = accelerator_->CreateVP8Picture();
  if (!curr_pic_)
    return kRanOutOfSurfaces;

  if (!DecodeAndOutputCurrentFrame())
    return kDecodeError;

  return kRanOutOfStreamData;
}

void VP8Decoder::RefreshReferenceFrames() {
  if (curr_frame_hdr_->IsKeyframe()) {
    last_frame_ = curr_pic_;
    golden_frame_ = curr_pic_;
    alt_frame_ = curr_pic_;
    return;
  }

  // Save current golden since we overwrite it here,
  // but may have to use it to update alt below.
  scoped_refptr<VP8Picture> curr_golden = golden_frame_;

  if (curr_frame_hdr_->refresh_golden_frame) {
    golden_frame_ = curr_pic_;
  } else {
    switch (curr_frame_hdr_->copy_buffer_to_golden) {
      case Vp8FrameHeader::COPY_LAST_TO_GOLDEN:
        DCHECK(last_frame_);
        golden_frame_ = last_frame_;
        break;

      case Vp8FrameHeader::COPY_ALT_TO_GOLDEN:
        DCHECK(alt_frame_);
        golden_frame_ = alt_frame_;
        break;
    }
  }

  if (curr_frame_hdr_->refresh_alternate_frame) {
    alt_frame_ = curr_pic_;
  } else {
    switch (curr_frame_hdr_->copy_buffer_to_alternate) {
      case Vp8FrameHeader::COPY_LAST_TO_ALT:
        DCHECK(last_frame_);
        alt_frame_ = last_frame_;
        break;

      case Vp8FrameHeader::COPY_GOLDEN_TO_ALT:
        DCHECK(curr_golden);
        alt_frame_ = curr_golden;
        break;
    }
  }

  if (curr_frame_hdr_->refresh_last)
    last_frame_ = curr_pic_;
}

bool VP8Decoder::DecodeAndOutputCurrentFrame() {
  DCHECK(!pic_size_.IsEmpty());
  DCHECK(curr_pic_);
  DCHECK(curr_frame_hdr_);

  if (curr_frame_hdr_->IsKeyframe()) {
    horizontal_scale_ = curr_frame_hdr_->horizontal_scale;
    vertical_scale_ = curr_frame_hdr_->vertical_scale;
  } else {
    // Populate fields from decoder state instead.
    curr_frame_hdr_->width = pic_size_.width();
    curr_frame_hdr_->height = pic_size_.height();
    curr_frame_hdr_->horizontal_scale = horizontal_scale_;
    curr_frame_hdr_->vertical_scale = vertical_scale_;
  }

  if (!accelerator_->SubmitDecode(curr_pic_, curr_frame_hdr_.get(), last_frame_,
                                  golden_frame_, alt_frame_))
    return false;

  if (curr_frame_hdr_->show_frame)
    if (!accelerator_->OutputPicture(curr_pic_))
      return false;

  RefreshReferenceFrames();

  curr_pic_ = nullptr;
  curr_frame_hdr_ = nullptr;
  curr_frame_start_ = nullptr;
  frame_size_ = 0;
  return true;
}

gfx::Size VP8Decoder::GetPicSize() const {
  return pic_size_;
}

size_t VP8Decoder::GetRequiredNumOfPictures() const {
  const size_t kVP8NumFramesActive = 4;
  const size_t kPicsInPipeline = limits::kMaxVideoFrames + 2;
  return kVP8NumFramesActive + kPicsInPipeline;
}

}  // namespace media
