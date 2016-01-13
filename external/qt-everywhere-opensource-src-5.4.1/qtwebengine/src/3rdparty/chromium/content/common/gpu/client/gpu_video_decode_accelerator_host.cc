// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif  // OS_WIN

using media::VideoDecodeAccelerator;
namespace content {

GpuVideoDecodeAcceleratorHost::GpuVideoDecodeAcceleratorHost(
    GpuChannelHost* channel,
    CommandBufferProxyImpl* impl)
    : channel_(channel),
      decoder_route_id_(MSG_ROUTING_NONE),
      client_(NULL),
      impl_(impl),
      weak_this_factory_(this) {
  DCHECK(channel_);
  DCHECK(impl_);
  impl_->AddDeletionObserver(this);
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {
  DCHECK(CalledOnValidThread());

  if (channel_ && decoder_route_id_ != MSG_ROUTING_NONE)
    channel_->RemoveRoute(decoder_route_id_);
  if (impl_)
    impl_->RemoveDeletionObserver(this);
}

bool GpuVideoDecodeAcceleratorHost::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAcceleratorHost, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                        OnBitstreamBufferProcessed)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                        OnProvidePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_PictureReady,
                        OnPictureReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_FlushDone,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ResetDone,
                        OnResetDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                        OnNotifyError)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer,
                        OnDismissPictureBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  // See OnNotifyError for why |this| mustn't be used after OnNotifyError might
  // have been called above.
  return handled;
}

void GpuVideoDecodeAcceleratorHost::OnChannelError() {
  DCHECK(CalledOnValidThread());
  if (channel_) {
    if (decoder_route_id_ != MSG_ROUTING_NONE)
      channel_->RemoveRoute(decoder_route_id_);
    channel_ = NULL;
  }
  DLOG(ERROR) << "OnChannelError()";
  PostNotifyError(PLATFORM_FAILURE);
}

bool GpuVideoDecodeAcceleratorHost::Initialize(media::VideoCodecProfile profile,
                                               Client* client) {
  DCHECK(CalledOnValidThread());
  client_ = client;

  if (!impl_)
    return false;

  int32 route_id = channel_->GenerateRouteID();
  channel_->AddRoute(route_id, weak_this_factory_.GetWeakPtr());

  bool succeeded = false;
  Send(new GpuCommandBufferMsg_CreateVideoDecoder(
      impl_->GetRouteID(), profile, route_id, &succeeded));

  if (!succeeded) {
    DLOG(ERROR) << "Send(GpuCommandBufferMsg_CreateVideoDecoder()) failed";
    PostNotifyError(PLATFORM_FAILURE);
    channel_->RemoveRoute(route_id);
    return false;
  }
  decoder_route_id_ = route_id;
  return true;
}

void GpuVideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;

  base::SharedMemoryHandle handle = channel_->ShareToGpuProcess(
      bitstream_buffer.handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    NOTREACHED() << "Failed to duplicate buffer handler";
    return;
  }

  Send(new AcceleratedVideoDecoderMsg_Decode(
      decoder_route_id_, handle, bitstream_buffer.id(),
      bitstream_buffer.size()));
}

void GpuVideoDecodeAcceleratorHost::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;
  // Rearrange data for IPC command.
  std::vector<int32> buffer_ids;
  std::vector<uint32> texture_ids;
  for (uint32 i = 0; i < buffers.size(); i++) {
    const media::PictureBuffer& buffer = buffers[i];
    if (buffer.size() != picture_buffer_dimensions_) {
      DLOG(ERROR) << "buffer.size() invalid: expected "
                  << picture_buffer_dimensions_.ToString()
                  << ", got " << buffer.size().ToString();
      PostNotifyError(INVALID_ARGUMENT);
      return;
    }
    texture_ids.push_back(buffer.texture_id());
    buffer_ids.push_back(buffer.id());
  }
  Send(new AcceleratedVideoDecoderMsg_AssignPictureBuffers(
      decoder_route_id_, buffer_ids, texture_ids));
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;
  Send(new AcceleratedVideoDecoderMsg_ReusePictureBuffer(
      decoder_route_id_, picture_buffer_id));
}

void GpuVideoDecodeAcceleratorHost::Flush() {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;
  Send(new AcceleratedVideoDecoderMsg_Flush(decoder_route_id_));
}

void GpuVideoDecodeAcceleratorHost::Reset() {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;
  Send(new AcceleratedVideoDecoderMsg_Reset(decoder_route_id_));
}

void GpuVideoDecodeAcceleratorHost::Destroy() {
  DCHECK(CalledOnValidThread());
  if (channel_)
    Send(new AcceleratedVideoDecoderMsg_Destroy(decoder_route_id_));
  client_ = NULL;
  delete this;
}

void GpuVideoDecodeAcceleratorHost::OnWillDeleteImpl() {
  DCHECK(CalledOnValidThread());
  impl_ = NULL;

  // The CommandBufferProxyImpl is going away; error out this VDA.
  OnChannelError();
}

void GpuVideoDecodeAcceleratorHost::PostNotifyError(Error error) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "PostNotifyError(): error=" << error;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuVideoDecodeAcceleratorHost::OnNotifyError,
                 weak_this_factory_.GetWeakPtr(),
                 error));
}

void GpuVideoDecodeAcceleratorHost::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  uint32 message_type = message->type();
  if (!channel_->Send(message)) {
    DLOG(ERROR) << "Send(" << message_type << ") failed";
    PostNotifyError(PLATFORM_FAILURE);
  }
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffer(
    uint32 num_requested_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  DCHECK(CalledOnValidThread());
  picture_buffer_dimensions_ = dimensions;
  if (client_) {
    client_->ProvidePictureBuffers(
        num_requested_buffers, dimensions, texture_target);
  }
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    int32 picture_buffer_id, int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  media::Picture picture(picture_buffer_id, bitstream_buffer_id);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnResetDone() {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyResetDone();
}

void GpuVideoDecodeAcceleratorHost::OnNotifyError(uint32 error) {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  weak_this_factory_.InvalidateWeakPtrs();

  // Client::NotifyError() may Destroy() |this|, so calling it needs to be the
  // last thing done on this stack!
  media::VideoDecodeAccelerator::Client* client = NULL;
  std::swap(client, client_);
  client->NotifyError(static_cast<media::VideoDecodeAccelerator::Error>(error));
}

}  // namespace content
