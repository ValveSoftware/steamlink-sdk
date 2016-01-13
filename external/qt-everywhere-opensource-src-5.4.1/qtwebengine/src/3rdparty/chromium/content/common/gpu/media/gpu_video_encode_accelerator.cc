// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_encode_accelerator.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
#include "content/common/gpu/media/v4l2_video_encode_accelerator.h"
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
#include "content/common/gpu/media/android_video_encode_accelerator.h"
#endif

namespace content {

static bool MakeDecoderContextCurrent(
    const base::WeakPtr<GpuCommandBufferStub> stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't MakeCurrent().";
    return false;
  }

  if (!stub->decoder()->MakeCurrent()) {
    DLOG(ERROR) << "Failed to MakeCurrent()";
    return false;
  }

  return true;
}

GpuVideoEncodeAccelerator::GpuVideoEncodeAccelerator(int32 host_route_id,
                                                     GpuCommandBufferStub* stub)
    : host_route_id_(host_route_id),
      stub_(stub),
      input_format_(media::VideoFrame::UNKNOWN),
      output_buffer_size_(0),
      weak_this_factory_(this) {
  stub_->AddDestructionObserver(this);
  make_context_current_ =
      base::Bind(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
}

GpuVideoEncodeAccelerator::~GpuVideoEncodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VEA has already been destroyed in there.
  DCHECK(!encoder_);
}

void GpuVideoEncodeAccelerator::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    IPC::Message* init_done_msg) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::Initialize(): "
              "input_format=" << input_format
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(!encoder_);

  if (!stub_->channel()->AddRoute(host_route_id_, this)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                   "failed to add route";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }

  if (input_visible_size.width() > media::limits::kMaxDimension ||
      input_visible_size.height() > media::limits::kMaxDimension ||
      input_visible_size.GetArea() > media::limits::kMaxCanvas) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                   "input_visible_size " << input_visible_size.ToString()
                << " too large";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }

  CreateEncoder();
  if (!encoder_) {
    DLOG(ERROR)
        << "GpuVideoEncodeAccelerator::Initialize(): VEA creation failed";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }
  if (!encoder_->Initialize(input_format,
                            input_visible_size,
                            output_profile,
                            initial_bitrate,
                            this)) {
    DLOG(ERROR)
        << "GpuVideoEncodeAccelerator::Initialize(): VEA initialization failed";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }
  input_format_ = input_format;
  input_visible_size_ = input_visible_size;
  SendCreateEncoderReply(init_done_msg, true);
}

bool GpuVideoEncodeAccelerator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAccelerator, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Encode, OnEncode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer,
                        OnUseOutputBitstreamBuffer)
    IPC_MESSAGE_HANDLER(
        AcceleratedVideoEncoderMsg_RequestEncodingParametersChange,
        OnRequestEncodingParametersChange)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  Send(new AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers(
      host_route_id_, input_count, input_coded_size, output_buffer_size));
  input_coded_size_ = input_coded_size;
  output_buffer_size_ = output_buffer_size;
}

void GpuVideoEncodeAccelerator::BitstreamBufferReady(int32 bitstream_buffer_id,
                                                     size_t payload_size,
                                                     bool key_frame) {
  Send(new AcceleratedVideoEncoderHostMsg_BitstreamBufferReady(
      host_route_id_, bitstream_buffer_id, payload_size, key_frame));
}

void GpuVideoEncodeAccelerator::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyError(host_route_id_, error));
}

void GpuVideoEncodeAccelerator::OnWillDestroyStub() {
  DCHECK(stub_);
  stub_->channel()->RemoveRoute(host_route_id_);
  stub_->RemoveDestructionObserver(this);
  encoder_.reset();
  delete this;
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GpuVideoEncodeAccelerator::GetSupportedProfiles() {
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles;

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
  profiles = V4L2VideoEncodeAccelerator::GetSupportedProfiles();
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  profiles = AndroidVideoEncodeAccelerator::GetSupportedProfiles();
#endif

  // TODO(sheu): return platform-specific profiles.
  return profiles;
}

void GpuVideoEncodeAccelerator::CreateEncoder() {
  DCHECK(!encoder_);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
  scoped_ptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kEncoder);
  if (!device.get())
    return;

  encoder_.reset(new V4L2VideoEncodeAccelerator(device.Pass()));
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  encoder_.reset(new AndroidVideoEncodeAccelerator());
#endif
}

void GpuVideoEncodeAccelerator::OnEncode(int32 frame_id,
                                         base::SharedMemoryHandle buffer_handle,
                                         uint32 buffer_size,
                                         bool force_keyframe) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnEncode(): frame_id=" << frame_id
           << ", buffer_size=" << buffer_size
           << ", force_keyframe=" << force_keyframe;
  if (!encoder_)
    return;
  if (frame_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): invalid frame_id="
                << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer_handle, true));
  if (!shm->Map(buffer_size)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                   "could not map frame_id=" << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  uint8* shm_memory = reinterpret_cast<uint8*>(shm->memory());
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalPackedMemory(
          input_format_,
          input_coded_size_,
          gfx::Rect(input_visible_size_),
          input_visible_size_,
          shm_memory,
          buffer_size,
          buffer_handle,
          base::TimeDelta(),
          // It's turtles all the way down...
          base::Bind(base::IgnoreResult(&base::MessageLoopProxy::PostTask),
                     base::MessageLoopProxy::current(),
                     FROM_HERE,
                     base::Bind(&GpuVideoEncodeAccelerator::EncodeFrameFinished,
                                weak_this_factory_.GetWeakPtr(),
                                frame_id,
                                base::Passed(&shm))));

  if (!frame) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                   "could not create VideoFrame for frame_id=" << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  encoder_->Encode(frame, force_keyframe);
}

void GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(
    int32 buffer_id,
    base::SharedMemoryHandle buffer_handle,
    uint32 buffer_size) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
              "buffer_id=" << buffer_id
           << ", buffer_size=" << buffer_size;
  if (!encoder_)
    return;
  if (buffer_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                   "invalid buffer_id=" << buffer_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  if (buffer_size < output_buffer_size_) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                   "buffer too small for buffer_id=" << buffer_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  encoder_->UseOutputBitstreamBuffer(
      media::BitstreamBuffer(buffer_id, buffer_handle, buffer_size));
}

void GpuVideoEncodeAccelerator::OnDestroy() {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnDestroy()";
  OnWillDestroyStub();
}

void GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(): "
              "bitrate=" << bitrate
           << ", framerate=" << framerate;
  if (!encoder_)
    return;
  encoder_->RequestEncodingParametersChange(bitrate, framerate);
}

void GpuVideoEncodeAccelerator::EncodeFrameFinished(
    int32 frame_id,
    scoped_ptr<base::SharedMemory> shm) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyInputDone(host_route_id_,
                                                          frame_id));
  // Just let shm fall out of scope.
}

void GpuVideoEncodeAccelerator::Send(IPC::Message* message) {
  stub_->channel()->Send(message);
}

void GpuVideoEncodeAccelerator::SendCreateEncoderReply(IPC::Message* message,
                                                       bool succeeded) {
  GpuCommandBufferMsg_CreateVideoEncoder::WriteReplyParams(message, succeeded);
  Send(message);
}

}  // namespace content
