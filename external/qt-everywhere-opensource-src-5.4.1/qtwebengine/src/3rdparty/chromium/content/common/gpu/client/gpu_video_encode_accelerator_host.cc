// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_video_encode_accelerator.h"
#include "media/base/video_frame.h"

namespace content {

#define NOTIFY_ERROR(error) \
  PostNotifyError(error);   \
  DLOG(ERROR)

GpuVideoEncodeAcceleratorHost::GpuVideoEncodeAcceleratorHost(
    GpuChannelHost* channel,
    CommandBufferProxyImpl* impl)
    : channel_(channel),
      encoder_route_id_(MSG_ROUTING_NONE),
      client_(NULL),
      impl_(impl),
      next_frame_id_(0),
      weak_this_factory_(this) {
  DCHECK(channel_);
  DCHECK(impl_);
  impl_->AddDeletionObserver(this);
}

GpuVideoEncodeAcceleratorHost::~GpuVideoEncodeAcceleratorHost() {
  DCHECK(CalledOnValidThread());
  if (channel_ && encoder_route_id_ != MSG_ROUTING_NONE)
    channel_->RemoveRoute(encoder_route_id_);
  if (impl_)
    impl_->RemoveDeletionObserver(this);
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GpuVideoEncodeAcceleratorHost::GetSupportedProfiles() {
  return GpuVideoEncodeAccelerator::GetSupportedProfiles();
}

bool GpuVideoEncodeAcceleratorHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAcceleratorHost, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers,
                        OnRequireBitstreamBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_NotifyInputDone,
                        OnNotifyInputDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_BitstreamBufferReady,
                        OnBitstreamBufferReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_NotifyError,
                        OnNotifyError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  // See OnNotifyError for why |this| mustn't be used after OnNotifyError might
  // have been called above.
  return handled;
}

void GpuVideoEncodeAcceleratorHost::OnChannelError() {
  DCHECK(CalledOnValidThread());
  if (channel_) {
    if (encoder_route_id_ != MSG_ROUTING_NONE)
      channel_->RemoveRoute(encoder_route_id_);
    channel_ = NULL;
  }
  NOTIFY_ERROR(kPlatformFailureError) << "OnChannelError()";
}

bool GpuVideoEncodeAcceleratorHost::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  DCHECK(CalledOnValidThread());
  client_ = client;
  if (!impl_) {
    DLOG(ERROR) << "impl_ destroyed";
    return false;
  }

  int32 route_id = channel_->GenerateRouteID();
  channel_->AddRoute(route_id, weak_this_factory_.GetWeakPtr());

  bool succeeded = false;
  Send(new GpuCommandBufferMsg_CreateVideoEncoder(impl_->GetRouteID(),
                                                  input_format,
                                                  input_visible_size,
                                                  output_profile,
                                                  initial_bitrate,
                                                  route_id,
                                                  &succeeded));
  if (!succeeded) {
    DLOG(ERROR) << "Send(GpuCommandBufferMsg_CreateVideoEncoder()) failed";
    channel_->RemoveRoute(route_id);
    return false;
  }
  encoder_route_id_ = route_id;
  return true;
}

void GpuVideoEncodeAcceleratorHost::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;

  if (!base::SharedMemory::IsHandleValid(frame->shared_memory_handle())) {
    NOTIFY_ERROR(kPlatformFailureError)
        << "Encode(): cannot encode frame not backed by shared memory";
    return;
  }
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    NOTIFY_ERROR(kPlatformFailureError)
        << "Encode(): failed to duplicate buffer handle for GPU process";
    return;
  }

  // We assume that planar frame data passed here is packed and contiguous.
  const size_t plane_count = media::VideoFrame::NumPlanes(frame->format());
  size_t frame_size = 0;
  for (size_t i = 0; i < plane_count; ++i) {
    // Cast DCHECK parameters to void* to avoid printing uint8* as a string.
    DCHECK_EQ(reinterpret_cast<void*>(frame->data(i)),
              reinterpret_cast<void*>((frame->data(0) + frame_size)))
        << "plane=" << i;
    frame_size += frame->stride(i) * frame->rows(i);
  }

  Send(new AcceleratedVideoEncoderMsg_Encode(
      encoder_route_id_, next_frame_id_, handle, frame_size, force_keyframe));
  frame_map_[next_frame_id_] = frame;

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_frame_id_ = (next_frame_id_ + 1) & 0x3FFFFFFF;
}

void GpuVideoEncodeAcceleratorHost::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;

  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(buffer.handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    NOTIFY_ERROR(kPlatformFailureError)
        << "UseOutputBitstreamBuffer(): failed to duplicate buffer handle "
           "for GPU process: buffer.id()=" << buffer.id();
    return;
  }
  Send(new AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer(
      encoder_route_id_, buffer.id(), handle, buffer.size()));
}

void GpuVideoEncodeAcceleratorHost::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;

  Send(new AcceleratedVideoEncoderMsg_RequestEncodingParametersChange(
      encoder_route_id_, bitrate, framerate));
}

void GpuVideoEncodeAcceleratorHost::Destroy() {
  DCHECK(CalledOnValidThread());
  if (channel_)
    Send(new AcceleratedVideoEncoderMsg_Destroy(encoder_route_id_));
  client_ = NULL;
  delete this;
}

void GpuVideoEncodeAcceleratorHost::OnWillDeleteImpl() {
  DCHECK(CalledOnValidThread());
  impl_ = NULL;

  // The CommandBufferProxyImpl is going away; error out this VEA.
  OnChannelError();
}

void GpuVideoEncodeAcceleratorHost::PostNotifyError(Error error) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "PostNotifyError(): error=" << error;
  // Post the error notification back to this thread, to avoid re-entrancy.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuVideoEncodeAcceleratorHost::OnNotifyError,
                 weak_this_factory_.GetWeakPtr(),
                 error));
}

void GpuVideoEncodeAcceleratorHost::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  uint32 message_type = message->type();
  if (!channel_->Send(message)) {
    NOTIFY_ERROR(kPlatformFailureError) << "Send(" << message_type
                                        << ") failed";
  }
}

void GpuVideoEncodeAcceleratorHost::OnRequireBitstreamBuffers(
    uint32 input_count,
    const gfx::Size& input_coded_size,
    uint32 output_buffer_size) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "OnRequireBitstreamBuffers(): input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  if (client_) {
    client_->RequireBitstreamBuffers(
        input_count, input_coded_size, output_buffer_size);
  }
}

void GpuVideoEncodeAcceleratorHost::OnNotifyInputDone(int32 frame_id) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnNotifyInputDone(): frame_id=" << frame_id;
  // Fun-fact: std::hash_map is not spec'd to be re-entrant; since freeing a
  // frame can trigger a further encode to be kicked off and thus an .insert()
  // back into the map, we separate the frame's dtor running from the .erase()
  // running by holding on to the frame temporarily.  This isn't "just
  // theoretical" - Android's std::hash_map crashes if we don't do this.
  scoped_refptr<media::VideoFrame> frame = frame_map_[frame_id];
  if (!frame_map_.erase(frame_id)) {
    DLOG(ERROR) << "OnNotifyInputDone(): "
                   "invalid frame_id=" << frame_id;
    // See OnNotifyError for why this needs to be the last thing in this
    // function.
    OnNotifyError(kPlatformFailureError);
    return;
  }
  frame = NULL;  // Not necessary but nice to be explicit; see fun-fact above.
}

void GpuVideoEncodeAcceleratorHost::OnBitstreamBufferReady(
    int32 bitstream_buffer_id,
    uint32 payload_size,
    bool key_frame) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnBitstreamBufferReady(): "
              "bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << payload_size
           << ", key_frame=" << key_frame;
  if (client_)
    client_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame);
}

void GpuVideoEncodeAcceleratorHost::OnNotifyError(Error error) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "OnNotifyError(): error=" << error;
  if (!client_)
    return;
  weak_this_factory_.InvalidateWeakPtrs();

  // Client::NotifyError() may Destroy() |this|, so calling it needs to be the
  // last thing done on this stack!
  media::VideoEncodeAccelerator::Client* client = NULL;
  std::swap(client_, client);
  client->NotifyError(error);
}

}  // namespace content
