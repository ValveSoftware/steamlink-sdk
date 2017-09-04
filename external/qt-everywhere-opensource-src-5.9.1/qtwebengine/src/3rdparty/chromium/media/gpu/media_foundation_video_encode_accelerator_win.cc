// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/media_foundation_video_encode_accelerator_win.h"

#pragma warning(push)
#pragma warning(disable : 4800)  // Disable warning for added padding.

#include <codecapi.h>
#include <mferror.h>
#include <mftransform.h>

#include <iterator>
#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "third_party/libyuv/include/libyuv.h"

using base::win::ScopedComPtr;
using media::mf::MediaBufferScopedPointer;

namespace media {

namespace {

const int32_t kDefaultTargetBitrate = 5000000;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 1920;
const size_t kMaxResolutionHeight = 1088;
const size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx
const size_t kOneMicrosecondInMFSampleTimeUnits = 10;
const size_t kOneSecondInMFSampleTimeUnits = 10000000;
const size_t kOutputSampleBufferSizeRatio = 4;

constexpr const wchar_t* const kMediaFoundationVideoEncoderDLLs[] = {
    L"mf.dll", L"mfplat.dll",
};

}  // namespace

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size, bool key_frame, base::TimeDelta timestamp)
      : keyframe(key_frame), capture_timestamp(timestamp), data_(size) {}

  uint8_t* memory() { return data_.data(); }

  int size() const { return static_cast<int>(data_.size()); }

  const bool keyframe;
  const base::TimeDelta capture_timestamp;

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(EncodeOutput);
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id,
                     std::unique_ptr<base::SharedMemory> shm,
                     size_t size)
      : id(id), shm(std::move(shm)), size(size) {}
  const int32_t id;
  const std::unique_ptr<base::SharedMemory> shm;
  const size_t size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBufferRef);
};

MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator()
    : main_client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoder_thread_("MFEncoderThread"),
      encoder_task_weak_factory_(this) {}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  SupportedProfiles profiles;

  target_bitrate_ = kDefaultTargetBitrate;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  input_visible_size_ = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  if (!CreateHardwareEncoderMFT() || !SetEncoderModes() ||
      !InitializeInputOutputSamples()) {
    ReleaseEncoderResources();
    DVLOG(1)
        << "Hardware encode acceleration is not available on this platform.";
    return profiles;
  }
  ReleaseEncoderResources();

  SupportedProfile profile;
  // More profiles can be supported here, but they should be available in SW
  // fallback as well.
  profile.profile = H264PROFILE_BASELINE;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  profiles.push_back(profile);
  return profiles;
}

bool MediaFoundationVideoEncodeAccelerator::Initialize(
    VideoPixelFormat format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    Client* client) {
  DVLOG(3) << __func__ << ": input_format=" << VideoPixelFormatToString(format)
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (PIXEL_FORMAT_I420 != format) {
    DLOG(ERROR) << "Input format not supported= "
                << VideoPixelFormatToString(format);
    return false;
  }

  if (H264PROFILE_BASELINE != output_profile) {
    DLOG(ERROR) << "Output profile not supported= " << output_profile;
    return false;
  }

  encoder_thread_.init_com_with_mta(false);
  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Failed spawning encoder thread.";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  if (!CreateHardwareEncoderMFT()) {
    DLOG(ERROR) << "Failed creating a hardware encoder MFT.";
    return false;
  }

  main_client_weak_factory_.reset(new base::WeakPtrFactory<Client>(client));
  main_client_ = main_client_weak_factory_->GetWeakPtr();
  input_visible_size_ = input_visible_size;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  target_bitrate_ = initial_bitrate;
  bitstream_buffer_size_ = input_visible_size.GetArea();
  u_plane_offset_ =
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kYPlane,
                            input_visible_size_)
          .GetArea();
  v_plane_offset_ =
      u_plane_offset_ +
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                            input_visible_size_)
          .GetArea();


  if (!SetEncoderModes()) {
    DLOG(ERROR) << "Failed setting encoder parameters.";
    return false;
  }

  if (!InitializeInputOutputSamples()) {
    DLOG(ERROR) << "Failed initializing input-output samples.";
    return false;
  }
  input_sample_.Attach(mf::CreateEmptySampleWithBuffer(
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, input_visible_size_), 2));
  output_sample_.Attach(mf::CreateEmptySampleWithBuffer(
      bitstream_buffer_size_ * kOutputSampleBufferSizeRatio, 2));

  HRESULT hr =
      encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set ProcessMessage", false);

  // Pin all client callbacks to the main task runner initially. It can be
  // reassigned by TryToSetupEncodeOnSeparateThread().
  if (!encode_client_task_runner_) {
    encode_client_task_runner_ = main_client_task_runner_;
    encode_client_ = main_client_;
  }

  main_client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::RequireBitstreamBuffers, main_client_,
                            kNumInputBuffers, input_visible_size_,
                            bitstream_buffer_size_));
  return SUCCEEDED(hr);
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encode_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaFoundationVideoEncodeAccelerator::EncodeTask,
                            encoder_task_weak_factory_.GetWeakPtr(), frame,
                            force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK(encode_client_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    NotifyError(kInvalidArgumentError);
    return;
  }

  std::unique_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer.handle(), false));
  if (!shm->Map(buffer.size())) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    NotifyError(kPlatformFailureError);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(shm), buffer.size()));
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
          encoder_task_weak_factory_.GetWeakPtr(), base::Passed(&buffer_ref)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate
           << ": framerate=" << framerate;
  DCHECK(encode_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaFoundationVideoEncodeAccelerator::
                     RequestEncodingParametersChangeTask,
                 encoder_task_weak_factory_.GetWeakPtr(), bitrate, framerate));
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Cancel all callbacks.
  main_client_weak_factory_.reset();

  if (encoder_thread_.IsRunning()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaFoundationVideoEncodeAccelerator::DestroyTask,
                   encoder_task_weak_factory_.GetWeakPtr()));
    encoder_thread_.Stop();
  }

  delete this;
}

bool MediaFoundationVideoEncodeAccelerator::TryToSetupEncodeOnSeparateThread(
    const base::WeakPtr<Client>& encode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& encode_task_runner) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  encode_client_ = encode_client;
  encode_client_task_runner_ = encode_task_runner;
  return true;
}

// static
void MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization() {
  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs)
    ::LoadLibrary(mfdll);
}

bool MediaFoundationVideoEncodeAccelerator::CreateHardwareEncoderMFT() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    DVLOG(ERROR) << "Windows versions earlier than 8 are not supported.";
    return false;
  }

  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      DVLOG(ERROR) << mfdll << " is required for encoding";
      return false;
    }
  }

  InitializeMediaFoundation();

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = MFVideoFormat_H264;

  base::win::ScopedCoMem<CLSID> CLSIDs;
  uint32_t count = 0;
  HRESULT hr = MFTEnum(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info,
                       &output_info, NULL, &CLSIDs, &count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't enumerate hardware encoder", false);
  RETURN_ON_FAILURE((count > 0), "No HW encoder found", false);
  DVLOG(3) << "HW encoder(s) found: " << count;
  hr = encoder_.CreateInstance(CLSIDs[0]);
  RETURN_ON_HR_FAILURE(hr, "Couldn't activate hardware encoder", false);
  RETURN_ON_FAILURE((encoder_.get() != nullptr),
                    "No HW encoder instance created", false);
  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputSamples() {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Initialize output parameters.
  HRESULT hr = MFCreateMediaType(imf_output_media_type_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Couldn't create media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  hr = MFSetAttributeRatio(imf_output_media_type_.get(), MF_MT_FRAME_RATE,
                           frame_rate_, kMaxFrameRateDenominator);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                         eAVEncH264VProfile_Base);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(0, imf_output_media_type_.get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(imf_input_media_type_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Couldn't create media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = MFSetAttributeRatio(imf_input_media_type_.get(), MF_MT_FRAME_RATE,
                           frame_rate_, kMaxFrameRateDenominator);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_input_media_type_.get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(0, imf_input_media_type_.get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return SUCCEEDED(hr);
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  RETURN_ON_FAILURE((encoder_.get() != nullptr),
                    "No HW encoder instance created", false);

  HRESULT hr = encoder_.QueryInterface(codec_api_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);
  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = eAVEncCommonRateControlMode_CBR;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);
  var.ulVal = target_bitrate_;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  var.ulVal = eAVEncAdaptiveMode_FrameRate;
  hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set FrameRate", false);
  var.vt = VT_BOOL;
  var.boolVal = VARIANT_TRUE;
  hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set LowLatencyMode", false);
  return SUCCEEDED(hr);
}

void MediaFoundationVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  main_client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::NotifyError, main_client_, error));
}

void MediaFoundationVideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  base::win::ScopedComPtr<IMFMediaBuffer> input_buffer;
  input_sample_->GetBufferByIndex(0, input_buffer.Receive());

  {
    MediaBufferScopedPointer scoped_buffer(input_buffer.get());
    DCHECK(scoped_buffer.get());
    libyuv::I420Copy(frame->visible_data(VideoFrame::kYPlane),
                     frame->stride(VideoFrame::kYPlane),
                     frame->visible_data(VideoFrame::kVPlane),
                     frame->stride(VideoFrame::kVPlane),
                     frame->visible_data(VideoFrame::kUPlane),
                     frame->stride(VideoFrame::kUPlane), scoped_buffer.get(),
                     frame->stride(VideoFrame::kYPlane),
                     scoped_buffer.get() + u_plane_offset_,
                     frame->stride(VideoFrame::kUPlane),
                     scoped_buffer.get() + v_plane_offset_,
                     frame->stride(VideoFrame::kVPlane),
                     input_visible_size_.width(), input_visible_size_.height());
  }

  input_sample_->SetSampleTime(frame->timestamp().InMicroseconds() *
                               kOneMicrosecondInMFSampleTimeUnits);
  input_sample_->SetSampleDuration(kOneSecondInMFSampleTimeUnits / frame_rate_);

  // Release frame after input is copied.
  frame = nullptr;

  HRESULT hr = encoder_->ProcessInput(0, input_sample_.get(), 0);
  // According to MSDN, if encoder returns MF_E_NOTACCEPTING, we need to try
  // processing the output. This error indicates that encoder does not accept
  // any more input data.
  if (hr == MF_E_NOTACCEPTING) {
    DVLOG(3) << "MF_E_NOTACCEPTING";
    ProcessOutput();
    hr = encoder_->ProcessInput(0, input_sample_.get(), 0);
    if (!SUCCEEDED(hr)) {
      NotifyError(kPlatformFailureError);
      RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
    }
  } else if (!SUCCEEDED(hr)) {
    NotifyError(kPlatformFailureError);
    RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
  }
  DVLOG(3) << "Sent for encode " << hr;

  ProcessOutput();
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutput() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = 0;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = NULL;
  output_data_buffer.pSample = output_sample_.get();
  DWORD status = 0;
  HRESULT hr = encoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    DVLOG(3) << "MF_E_TRANSFORM_NEED_MORE_INPUT";
    return;
  }
  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  hr = output_sample_->GetBufferByIndex(0, output_buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );
  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_sample_->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp = base::TimeDelta::FromMicroseconds(
        sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_sample_.get(), MFSampleExtension_CleanPoint, false);
  DVLOG(3) << "We HAVE encoded data with size:" << size << " keyframe "
           << keyframe;

  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";
    // We need to copy the output so that encoding can continue.
    std::unique_ptr<EncodeOutput> encode_output(
        new EncodeOutput(size, keyframe, timestamp));
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.get());
    memcpy(buffer_ref->shm->memory(), scoped_buffer.get(), size);
  }

  encode_client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::BitstreamBufferReady, encode_client_,
                            buffer_ref->id, size, keyframe, timestamp));

  // Keep calling ProcessOutput recursively until MF_E_TRANSFORM_NEED_MORE_INPUT
  // is returned to flush out all the output.
  ProcessOutput();
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::EncodeOutput>
        encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void MediaFoundationVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
        buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  memcpy(buffer_ref->shm->memory(), encode_output->memory(),
         encode_output->size());
  encode_client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::BitstreamBufferReady, encode_client_, buffer_ref->id,
                 encode_output->size(), encode_output->keyframe,
                 encode_output->capture_timestamp));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  frame_rate_ = framerate ? framerate : 1;
  target_bitrate_ = bitrate ? bitrate : 1;

  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = target_bitrate_;
  HRESULT hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", );

  hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", );
  hr = MFSetAttributeRatio(imf_output_media_type_.get(), MF_MT_FRAME_RATE,
                           frame_rate_, kMaxFrameRateDenominator);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output type params", );
}

void MediaFoundationVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  ReleaseEncoderResources();
}

void MediaFoundationVideoEncodeAccelerator::ReleaseEncoderResources() {
  encoder_.Release();
  codec_api_.Release();
  imf_input_media_type_.Release();
  imf_output_media_type_.Release();
  input_sample_.Release();
  output_sample_.Release();
}

}  // namespace content
