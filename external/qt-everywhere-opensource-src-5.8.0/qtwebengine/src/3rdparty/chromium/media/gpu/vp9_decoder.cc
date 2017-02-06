// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_decoder.h"

#include <memory>

#include "base/logging.h"
#include "media/base/limits.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

VP9Decoder::VP9Accelerator::VP9Accelerator() {}

VP9Decoder::VP9Accelerator::~VP9Accelerator() {}

VP9Decoder::VP9Decoder(VP9Accelerator* accelerator)
    : state_(kNeedStreamMetadata), accelerator_(accelerator) {
  DCHECK(accelerator_);
  ref_frames_.resize(kVp9NumRefFrames);
}

VP9Decoder::~VP9Decoder() {}

void VP9Decoder::SetStream(const uint8_t* ptr, size_t size) {
  DCHECK(ptr);
  DCHECK(size);

  DVLOG(4) << "New input stream at: " << (void*)ptr << " size: " << size;
  parser_.SetStream(ptr, size);
}

bool VP9Decoder::Flush() {
  DVLOG(2) << "Decoder flush";
  Reset();
  return true;
}

void VP9Decoder::Reset() {
  curr_frame_hdr_ = nullptr;
  for (auto& ref_frame : ref_frames_)
    ref_frame = nullptr;

  parser_.Reset();

  if (state_ == kDecoding)
    state_ = kAfterReset;
}

VP9Decoder::DecodeResult VP9Decoder::Decode() {
  while (1) {
    // Read a new frame header if one is not awaiting decoding already.
    if (!curr_frame_hdr_) {
      std::unique_ptr<Vp9FrameHeader> hdr(new Vp9FrameHeader());
      Vp9Parser::Result res = parser_.ParseNextFrame(hdr.get());
      switch (res) {
        case Vp9Parser::kOk:
          curr_frame_hdr_.reset(hdr.release());
          break;

        case Vp9Parser::kEOStream:
          return kRanOutOfStreamData;

        case Vp9Parser::kInvalidStream:
          DVLOG(1) << "Error parsing stream";
          SetError();
          return kDecodeError;
      }
    }

    if (state_ != kDecoding) {
      // Not kDecoding, so we need a resume point (a keyframe), as we are after
      // reset or at the beginning of the stream. Drop anything that is not
      // a keyframe in such case, and continue looking for a keyframe.
      if (curr_frame_hdr_->IsKeyframe()) {
        state_ = kDecoding;
      } else {
        curr_frame_hdr_.reset();
        continue;
      }
    }

    if (curr_frame_hdr_->show_existing_frame) {
      // This frame header only instructs us to display one of the
      // previously-decoded frames, but has no frame data otherwise. Display
      // and continue decoding subsequent frames.
      size_t frame_to_show = curr_frame_hdr_->frame_to_show;
      if (frame_to_show >= ref_frames_.size() || !ref_frames_[frame_to_show]) {
        DVLOG(1) << "Request to show an invalid frame";
        SetError();
        return kDecodeError;
      }

      if (!accelerator_->OutputPicture(ref_frames_[frame_to_show])) {
        SetError();
        return kDecodeError;
      }

      curr_frame_hdr_.reset();
      continue;
    }

    gfx::Size new_pic_size(curr_frame_hdr_->width, curr_frame_hdr_->height);
    DCHECK(!new_pic_size.IsEmpty());

    if (new_pic_size != pic_size_) {
      DVLOG(1) << "New resolution: " << new_pic_size.ToString();

      if (!curr_frame_hdr_->IsKeyframe()) {
        // TODO(posciak): This is doable, but requires a few modifications to
        // VDA implementations to allow multiple picture buffer sets in flight.
        DVLOG(1) << "Resolution change currently supported for keyframes only";
        SetError();
        return kDecodeError;
      }

      // TODO(posciak): This requires us to be on a keyframe (see above) and is
      // required, because VDA clients expect all surfaces to be returned before
      // they can cycle surface sets after receiving kAllocateNewSurfaces.
      // This is only an implementation detail of VDAs and can be improved.
      for (auto& ref_frame : ref_frames_)
        ref_frame = nullptr;

      pic_size_ = new_pic_size;
      return kAllocateNewSurfaces;
    }

    scoped_refptr<VP9Picture> pic = accelerator_->CreateVP9Picture();
    if (!pic)
      return kRanOutOfSurfaces;

    pic->frame_hdr.reset(curr_frame_hdr_.release());

    if (!DecodeAndOutputPicture(pic)) {
      SetError();
      return kDecodeError;
    }
  }
}

void VP9Decoder::RefreshReferenceFrames(const scoped_refptr<VP9Picture>& pic) {
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    DCHECK(!pic->frame_hdr->IsKeyframe() || pic->frame_hdr->RefreshFlag(i));
    if (pic->frame_hdr->RefreshFlag(i))
      ref_frames_[i] = pic;
  }
}

bool VP9Decoder::DecodeAndOutputPicture(scoped_refptr<VP9Picture> pic) {
  DCHECK(!pic_size_.IsEmpty());
  DCHECK(pic->frame_hdr);

  if (!accelerator_->SubmitDecode(pic, parser_.GetSegmentation(),
                                  parser_.GetLoopFilter(), ref_frames_))
    return false;

  if (pic->frame_hdr->show_frame) {
    if (!accelerator_->OutputPicture(pic))
      return false;
  }

  RefreshReferenceFrames(pic);
  return true;
}

void VP9Decoder::SetError() {
  Reset();
  state_ = kError;
}

gfx::Size VP9Decoder::GetPicSize() const {
  return pic_size_;
}

size_t VP9Decoder::GetRequiredNumOfPictures() const {
  // kMaxVideoFrames to keep higher level media pipeline populated, +2 for the
  // pictures being parsed and decoded currently.
  return limits::kMaxVideoFrames + kVp9NumRefFrames + 2;
}

}  // namespace media
