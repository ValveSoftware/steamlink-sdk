// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vt_video_decode_accelerator.h"

namespace content {

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(CGLContextObj cgl_context)
    : loop_proxy_(base::MessageLoopProxy::current()),
      cgl_context_(cgl_context),
      client_(NULL),
      weak_this_factory_(this) {
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
}

bool VTVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile,
    Client* client) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << __FUNCTION__;
  client_ = client;

  // Only H.264 is supported.
  if (profile < media::H264PROFILE_MIN || profile > media::H264PROFILE_MAX)
    return false;

  // Prevent anyone from using VTVideoDecoder for now. http://crbug.com/133828
  return false;
}

void VTVideoDecodeAccelerator::Decode(const media::BitstreamBuffer& bitstream) {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& pictures) {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::Flush() {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::Reset() {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::Destroy() {
  DCHECK(CalledOnValidThread());
}

bool VTVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

}  // namespace content
