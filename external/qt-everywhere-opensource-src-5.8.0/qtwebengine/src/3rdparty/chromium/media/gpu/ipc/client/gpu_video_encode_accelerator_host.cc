// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/client/gpu_video_encode_accelerator_host.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

GpuVideoEncodeAcceleratorHost::GpuVideoEncodeAcceleratorHost(
    gpu::CommandBufferProxyImpl* impl)
    : channel_(impl->channel()),
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
  PostNotifyError(FROM_HERE, kPlatformFailureError, "OnChannelError()");
}

VideoEncodeAccelerator::SupportedProfiles
GpuVideoEncodeAcceleratorHost::GetSupportedProfiles() {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return VideoEncodeAccelerator::SupportedProfiles();
  return GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
      channel_->gpu_info().video_encode_accelerator_supported_profiles);
}

bool GpuVideoEncodeAcceleratorHost::Initialize(
    VideoPixelFormat input_format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    Client* client) {
  DCHECK(CalledOnValidThread());
  client_ = client;
  if (!impl_) {
    DLOG(ERROR) << "impl_ destroyed";
    return false;
  }

  int32_t route_id = channel_->GenerateRouteID();
  channel_->AddRoute(route_id, weak_this_factory_.GetWeakPtr());

  CreateVideoEncoderParams params;
  params.input_format = input_format;
  params.input_visible_size = input_visible_size;
  params.output_profile = output_profile;
  params.initial_bitrate = initial_bitrate;
  params.encoder_route_id = route_id;
  bool succeeded = false;
  Send(new GpuCommandBufferMsg_CreateVideoEncoder(impl_->route_id(), params,
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
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(PIXEL_FORMAT_I420, frame->format());
  if (!channel_)
    return;

  switch (frame->storage_type()) {
    case VideoFrame::STORAGE_SHMEM:
      EncodeSharedMemoryFrame(frame, force_keyframe);
      break;
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFERS:
      EncodeGpuMemoryBufferFrame(frame, force_keyframe);
      break;
    default:
      PostNotifyError(FROM_HERE, kPlatformFailureError,
                      "Encode(): cannot encode frame with invalid handles");
      return;
  }

  frame_map_[next_frame_id_] = frame;

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_frame_id_ = (next_frame_id_ + 1) & 0x3FFFFFFF;
}

void GpuVideoEncodeAcceleratorHost::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return;

  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(buffer.handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    PostNotifyError(
        FROM_HERE, kPlatformFailureError,
        base::StringPrintf("UseOutputBitstreamBuffer(): failed to duplicate "
                           "buffer handle for GPU process: buffer.id()=%d",
                           buffer.id()));
    return;
  }
  Send(new AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer(
      encoder_route_id_, buffer.id(), handle, buffer.size()));
}

void GpuVideoEncodeAcceleratorHost::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
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

  // The gpu::CommandBufferProxyImpl is going away; error out this VEA.
  OnChannelError();
}

void GpuVideoEncodeAcceleratorHost::EncodeGpuMemoryBufferFrame(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  DCHECK_EQ(VideoFrame::NumPlanes(PIXEL_FORMAT_I420),
            frame->gpu_memory_buffer_handles().size());
  AcceleratedVideoEncoderMsg_Encode_Params2 params;
  params.frame_id = next_frame_id_;
  params.timestamp = frame->timestamp();
  bool requires_sync_point = false;
  for (const auto& handle : frame->gpu_memory_buffer_handles()) {
    gfx::GpuMemoryBufferHandle new_handle =
        channel_->ShareGpuMemoryBufferToGpuProcess(handle,
                                                   &requires_sync_point);
    if (new_handle.is_null()) {
      PostNotifyError(FROM_HERE, kPlatformFailureError,
                      "EncodeGpuMemoryBufferFrame(): failed to share gpu "
                      "memory buffer handle for gpu process");
      return;
    }
    params.gpu_memory_buffer_handles.push_back(new_handle);
  }
  params.size = frame->coded_size();
  params.force_keyframe = force_keyframe;

  Send(new AcceleratedVideoEncoderMsg_Encode2(encoder_route_id_, params));
}

void GpuVideoEncodeAcceleratorHost::EncodeSharedMemoryFrame(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  if (!base::SharedMemory::IsHandleValid(frame->shared_memory_handle())) {
    PostNotifyError(FROM_HERE, kPlatformFailureError,
                    "EncodeSharedMemory(): cannot encode frame with invalid "
                    "shared memory handle");
    return;
  }

  AcceleratedVideoEncoderMsg_Encode_Params params;
  params.frame_id = next_frame_id_;
  params.timestamp = frame->timestamp();
  params.buffer_handle =
      channel_->ShareToGpuProcess(frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(params.buffer_handle)) {
    PostNotifyError(FROM_HERE, kPlatformFailureError,
                    "Encode(): failed to duplicate shared memory buffer handle "
                    "for GPU process");
    return;
  }
  params.buffer_offset =
      base::checked_cast<uint32_t>(frame->shared_memory_offset());
  params.buffer_size =
      VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  params.force_keyframe = force_keyframe;

  Send(new AcceleratedVideoEncoderMsg_Encode(encoder_route_id_, params));
}

void GpuVideoEncodeAcceleratorHost::PostNotifyError(
    const tracked_objects::Location& location,
    Error error,
    const std::string& message) {
  DCHECK(CalledOnValidThread());
  DLOG(ERROR) << "Error from " << location.function_name() << "("
              << location.file_name() << ":" << location.line_number() << ") "
              << message << " (error = " << error << ")";
  // Post the error notification back to this thread, to avoid re-entrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&GpuVideoEncodeAcceleratorHost::OnNotifyError,
                            weak_this_factory_.GetWeakPtr(), error));
}

void GpuVideoEncodeAcceleratorHost::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  uint32_t message_type = message->type();
  if (!channel_->Send(message)) {
    PostNotifyError(FROM_HERE, kPlatformFailureError,
                    base::StringPrintf("Send(%d) failed", message_type));
  }
}

void GpuVideoEncodeAcceleratorHost::OnRequireBitstreamBuffers(
    uint32_t input_count,
    const gfx::Size& input_coded_size,
    uint32_t output_buffer_size) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "OnRequireBitstreamBuffers(): input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  if (client_) {
    client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                     output_buffer_size);
  }
}

void GpuVideoEncodeAcceleratorHost::OnNotifyInputDone(int32_t frame_id) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnNotifyInputDone(): frame_id=" << frame_id;
  // Fun-fact: std::hash_map is not spec'd to be re-entrant; since freeing a
  // frame can trigger a further encode to be kicked off and thus an .insert()
  // back into the map, we separate the frame's dtor running from the .erase()
  // running by holding on to the frame temporarily.  This isn't "just
  // theoretical" - Android's std::hash_map crashes if we don't do this.
  scoped_refptr<VideoFrame> frame = frame_map_[frame_id];
  if (!frame_map_.erase(frame_id)) {
    DLOG(ERROR) << "OnNotifyInputDone(): "
                   "invalid frame_id="
                << frame_id;
    // See OnNotifyError for why this needs to be the last thing in this
    // function.
    OnNotifyError(kPlatformFailureError);
    return;
  }
  frame = NULL;  // Not necessary but nice to be explicit; see fun-fact above.
}

void GpuVideoEncodeAcceleratorHost::OnBitstreamBufferReady(
    int32_t bitstream_buffer_id,
    uint32_t payload_size,
    bool key_frame,
    base::TimeDelta timestamp) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnBitstreamBufferReady(): "
              "bitstream_buffer_id="
           << bitstream_buffer_id << ", payload_size=" << payload_size
           << ", key_frame=" << key_frame;
  if (client_)
    client_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame,
                                  timestamp);
}

void GpuVideoEncodeAcceleratorHost::OnNotifyError(Error error) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "OnNotifyError(): error=" << error;
  if (!client_)
    return;
  weak_this_factory_.InvalidateWeakPtrs();

  // Client::NotifyError() may Destroy() |this|, so calling it needs to be the
  // last thing done on this stack!
  VideoEncodeAccelerator::Client* client = NULL;
  std::swap(client_, client);
  client->NotifyError(error);
}

}  // namespace media
