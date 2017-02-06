// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"

#include <memory>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/ipc/common/media_messages.h"

#if defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "media/gpu/v4l2_video_encode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_video_encode_accelerator.h"
#endif
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
#include "media/gpu/android_video_encode_accelerator.h"
#elif defined(OS_MACOSX)
#include "media/gpu/vt_video_encode_accelerator_mac.h"
#endif

namespace media {

static bool MakeDecoderContextCurrent(
    const base::WeakPtr<gpu::GpuCommandBufferStub> stub) {
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

GpuVideoEncodeAccelerator::GpuVideoEncodeAccelerator(
    int32_t host_route_id,
    gpu::GpuCommandBufferStub* stub)
    : host_route_id_(host_route_id),
      stub_(stub),
      input_format_(PIXEL_FORMAT_UNKNOWN),
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

bool GpuVideoEncodeAccelerator::Initialize(VideoPixelFormat input_format,
                                           const gfx::Size& input_visible_size,
                                           VideoCodecProfile output_profile,
                                           uint32_t initial_bitrate) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::Initialize(): "
           << "input_format=" << input_format
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(!encoder_);

  if (!stub_->channel()->AddRoute(host_route_id_, stub_->stream_id(), this)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                   "failed to add route";
    return false;
  }

  if (input_visible_size.width() > limits::kMaxDimension ||
      input_visible_size.height() > limits::kMaxDimension ||
      input_visible_size.GetArea() > limits::kMaxCanvas) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                << "input_visible_size " << input_visible_size.ToString()
                << " too large";
    return false;
  }

  const gpu::GpuPreferences& gpu_preferences =
      stub_->channel()->gpu_channel_manager()->gpu_preferences();

  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp> create_vea_fps =
      CreateVEAFps(gpu_preferences);
  // Try all possible encoders and use the first successful encoder.
  for (size_t i = 0; i < create_vea_fps.size(); ++i) {
    encoder_ = (*create_vea_fps[i])();
    if (encoder_ &&
        encoder_->Initialize(input_format, input_visible_size, output_profile,
                             initial_bitrate, this)) {
      input_format_ = input_format;
      input_visible_size_ = input_visible_size;
      return true;
    }
  }
  encoder_.reset();
  DLOG(ERROR)
      << "GpuVideoEncodeAccelerator::Initialize(): VEA initialization failed";
  return false;
}

bool GpuVideoEncodeAccelerator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAccelerator, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Encode, OnEncode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Encode2, OnEncode2)
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

void GpuVideoEncodeAccelerator::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    size_t payload_size,
    bool key_frame,
    base::TimeDelta timestamp) {
  Send(new AcceleratedVideoEncoderHostMsg_BitstreamBufferReady(
      host_route_id_, bitstream_buffer_id, payload_size, key_frame, timestamp));
}

void GpuVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
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
gpu::VideoEncodeAcceleratorSupportedProfiles
GpuVideoEncodeAccelerator::GetSupportedProfiles(
    const gpu::GpuPreferences& gpu_preferences) {
  VideoEncodeAccelerator::SupportedProfiles profiles;
  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp> create_vea_fps =
      CreateVEAFps(gpu_preferences);

  for (size_t i = 0; i < create_vea_fps.size(); ++i) {
    std::unique_ptr<VideoEncodeAccelerator> encoder = (*create_vea_fps[i])();
    if (!encoder)
      continue;
    VideoEncodeAccelerator::SupportedProfiles vea_profiles =
        encoder->GetSupportedProfiles();
    GpuVideoAcceleratorUtil::InsertUniqueEncodeProfiles(vea_profiles,
                                                        &profiles);
  }
  return GpuVideoAcceleratorUtil::ConvertMediaToGpuEncodeProfiles(profiles);
}

// static
std::vector<GpuVideoEncodeAccelerator::CreateVEAFp>
GpuVideoEncodeAccelerator::CreateVEAFps(
    const gpu::GpuPreferences& gpu_preferences) {
  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp> create_vea_fps;
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateV4L2VEA);
#endif
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  if (!gpu_preferences.disable_vaapi_accelerated_video_encode)
    create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateVaapiVEA);
#endif
#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  if (!gpu_preferences.disable_web_rtc_hw_encoding)
    create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateAndroidVEA);
#endif
#if defined(OS_MACOSX)
  create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateVTVEA);
#endif
  return create_vea_fps;
}

#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
// static
std::unique_ptr<VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateV4L2VEA() {
  std::unique_ptr<VideoEncodeAccelerator> encoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kEncoder);
  if (device)
    encoder.reset(new V4L2VideoEncodeAccelerator(device));
  return encoder;
}
#endif

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
// static
std::unique_ptr<VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateVaapiVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new VaapiVideoEncodeAccelerator());
}
#endif

#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
// static
std::unique_ptr<VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateAndroidVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new AndroidVideoEncodeAccelerator());
}
#endif

#if defined(OS_MACOSX)
// static
std::unique_ptr<VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateVTVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new VTVideoEncodeAccelerator());
}
#endif

void GpuVideoEncodeAccelerator::OnEncode(
    const AcceleratedVideoEncoderMsg_Encode_Params& params) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnEncode: frame_id = "
           << params.frame_id << ", buffer_size=" << params.buffer_size
           << ", force_keyframe=" << params.force_keyframe;
  DCHECK_EQ(PIXEL_FORMAT_I420, input_format_);

  // Wrap into a SharedMemory in the beginning, so that |params.buffer_handle|
  // is cleaned properly in case of an early return.
  std::unique_ptr<base::SharedMemory> shm(
      new base::SharedMemory(params.buffer_handle, true));

  if (!encoder_)
    return;

  if (params.frame_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): invalid "
                << "frame_id=" << params.frame_id;
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  const uint32_t aligned_offset =
      params.buffer_offset % base::SysInfo::VMAllocationGranularity();
  base::CheckedNumeric<off_t> map_offset = params.buffer_offset;
  map_offset -= aligned_offset;
  base::CheckedNumeric<size_t> map_size = params.buffer_size;
  map_size += aligned_offset;

  if (!map_offset.IsValid() || !map_size.IsValid()) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode():"
                << " invalid (buffer_offset,buffer_size)";
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  if (!shm->MapAt(map_offset.ValueOrDie(), map_size.ValueOrDie())) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                << "could not map frame_id=" << params.frame_id;
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  uint8_t* shm_memory =
      reinterpret_cast<uint8_t*>(shm->memory()) + aligned_offset;
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalSharedMemory(
      input_format_, input_coded_size_, gfx::Rect(input_visible_size_),
      input_visible_size_, shm_memory, params.buffer_size, params.buffer_handle,
      params.buffer_offset, params.timestamp);
  if (!frame) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                << "could not create a frame";
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  frame->AddDestructionObserver(BindToCurrentLoop(base::Bind(
      &GpuVideoEncodeAccelerator::EncodeFrameFinished,
      weak_this_factory_.GetWeakPtr(), params.frame_id, base::Passed(&shm))));
  encoder_->Encode(frame, params.force_keyframe);
}

void GpuVideoEncodeAccelerator::OnEncode2(
    const AcceleratedVideoEncoderMsg_Encode_Params2& params) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnEncode2: "
           << "frame_id = " << params.frame_id
           << ", size=" << params.size.ToString()
           << ", force_keyframe=" << params.force_keyframe
           << ", handle type=" << params.gpu_memory_buffer_handles[0].type;
  // Encoding GpuMemoryBuffer backed frames is not supported.
  NOTREACHED();
}

void GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(
    int32_t buffer_id,
    base::SharedMemoryHandle buffer_handle,
    uint32_t buffer_size) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
           << "buffer_id=" << buffer_id << ", buffer_size=" << buffer_size;
  if (!encoder_)
    return;
  if (buffer_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                << "invalid buffer_id=" << buffer_id;
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  if (buffer_size < output_buffer_size_) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                << "buffer too small for buffer_id=" << buffer_id;
    NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  encoder_->UseOutputBitstreamBuffer(
      BitstreamBuffer(buffer_id, buffer_handle, buffer_size));
}

void GpuVideoEncodeAccelerator::OnDestroy() {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnDestroy()";
  OnWillDestroyStub();
}

void GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(): "
           << "bitrate=" << bitrate << ", framerate=" << framerate;
  if (!encoder_)
    return;
  encoder_->RequestEncodingParametersChange(bitrate, framerate);
}

void GpuVideoEncodeAccelerator::EncodeFrameFinished(
    int32_t frame_id,
    std::unique_ptr<base::SharedMemory> shm) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyInputDone(host_route_id_,
                                                          frame_id));
  // Just let |shm| fall out of scope.
}

void GpuVideoEncodeAccelerator::Send(IPC::Message* message) {
  stub_->channel()->Send(message);
}

}  // namespace media
