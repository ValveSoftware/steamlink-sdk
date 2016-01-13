// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_auhal_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/time/time.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_pull_fifo.h"

namespace media {

static void WrapBufferList(AudioBufferList* buffer_list,
                           AudioBus* bus,
                           int frames) {
  DCHECK(buffer_list);
  DCHECK(bus);
  const int channels = bus->channels();
  const int buffer_list_channels = buffer_list->mNumberBuffers;
  CHECK_EQ(channels, buffer_list_channels);

  // Copy pointers from AudioBufferList.
  for (int i = 0; i < channels; ++i) {
    bus->SetChannelData(
        i, static_cast<float*>(buffer_list->mBuffers[i].mData));
  }

  // Finally set the actual length.
  bus->set_frames(frames);
}

AUHALStream::AUHALStream(
    AudioManagerMac* manager,
    const AudioParameters& params,
    AudioDeviceID device)
    : manager_(manager),
      params_(params),
      output_channels_(params_.channels()),
      number_of_frames_(params_.frames_per_buffer()),
      source_(NULL),
      device_(device),
      audio_unit_(0),
      volume_(1),
      hardware_latency_frames_(0),
      stopped_(false),
      current_hardware_pending_bytes_(0) {
  // We must have a manager.
  DCHECK(manager_);

  VLOG(1) << "AUHALStream::AUHALStream()";
  VLOG(1) << "Device: " << device;
  VLOG(1) << "Output channels: " << output_channels_;
  VLOG(1) << "Sample rate: " << params_.sample_rate();
  VLOG(1) << "Buffer size: " << number_of_frames_;
}

AUHALStream::~AUHALStream() {
}

bool AUHALStream::Open() {
  // Get the total number of output channels that the
  // hardware supports.
  int device_output_channels;
  bool got_output_channels = AudioManagerMac::GetDeviceChannels(
      device_,
      kAudioDevicePropertyScopeOutput,
      &device_output_channels);

  // Sanity check the requested output channels.
  if (!got_output_channels ||
      output_channels_ <= 0 || output_channels_ > device_output_channels) {
    LOG(ERROR) << "AudioDevice does not support requested output channels.";
    return false;
  }

  // The requested sample-rate must match the hardware sample-rate.
  int sample_rate = AudioManagerMac::HardwareSampleRateForDevice(device_);

  if (sample_rate != params_.sample_rate()) {
    LOG(ERROR) << "Requested sample-rate: " << params_.sample_rate()
               << " must match the hardware sample-rate: " << sample_rate;
    return false;
  }

  // The output bus will wrap the AudioBufferList given to us in
  // the Render() callback.
  DCHECK_GT(output_channels_, 0);
  output_bus_ = AudioBus::CreateWrapper(output_channels_);

  bool configured = ConfigureAUHAL();
  if (configured)
    hardware_latency_frames_ = GetHardwareLatency();

  return configured;
}

void AUHALStream::Close() {
  if (audio_unit_) {
    OSStatus result = AudioUnitUninitialize(audio_unit_);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "AudioUnitUninitialize() failed.";
    result = AudioComponentInstanceDispose(audio_unit_);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "AudioComponentInstanceDispose() failed.";
  }

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AUHALStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  if (!audio_unit_) {
    DLOG(ERROR) << "Open() has not been called successfully";
    return;
  }

  // Check if we should defer Start() for http://crbug.com/160920.
  if (manager_->ShouldDeferStreamStart()) {
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(
        base::Bind(&AUHALStream::Start, base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, deferred_start_cb_.callback(), base::TimeDelta::FromSeconds(
            AudioManagerMac::kStartDelayInSecsForPowerEvents));
    return;
  }

  stopped_ = false;
  audio_fifo_.reset();
  {
    base::AutoLock auto_lock(source_lock_);
    source_ = callback;
  }

  OSStatus result = AudioOutputUnitStart(audio_unit_);
  if (result == noErr)
    return;

  Stop();
  OSSTATUS_DLOG(ERROR, result) << "AudioOutputUnitStart() failed.";
  callback->OnError(this);
}

void AUHALStream::Stop() {
  deferred_start_cb_.Cancel();
  if (stopped_)
    return;

  OSStatus result = AudioOutputUnitStop(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioOutputUnitStop() failed.";
  if (result != noErr)
    source_->OnError(this);

  base::AutoLock auto_lock(source_lock_);
  source_ = NULL;
  stopped_ = true;
}

void AUHALStream::SetVolume(double volume) {
  volume_ = static_cast<float>(volume);
}

void AUHALStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Pulls on our provider to get rendered audio stream.
// Note to future hackers of this function: Do not add locks which can
// be contended in the middle of stream processing here (starting and stopping
// the stream are ok) because this is running on a real-time thread.
OSStatus AUHALStream::Render(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* output_time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* data) {
  TRACE_EVENT0("audio", "AUHALStream::Render");

  // If the stream parameters change for any reason, we need to insert a FIFO
  // since the OnMoreData() pipeline can't handle frame size changes.
  if (number_of_frames != number_of_frames_) {
    // Create a FIFO on the fly to handle any discrepancies in callback rates.
    if (!audio_fifo_) {
      VLOG(1) << "Audio frame size changed from " << number_of_frames_ << " to "
              << number_of_frames << "; adding FIFO to compensate.";
      audio_fifo_.reset(new AudioPullFifo(
          output_channels_,
          number_of_frames_,
          base::Bind(&AUHALStream::ProvideInput, base::Unretained(this))));
    }
  }

  // Make |output_bus_| wrap the output AudioBufferList.
  WrapBufferList(data, output_bus_.get(), number_of_frames);

  // Update the playout latency.
  const double playout_latency_frames = GetPlayoutLatency(output_time_stamp);
  current_hardware_pending_bytes_ = static_cast<uint32>(
      (playout_latency_frames + 0.5) * params_.GetBytesPerFrame());

  if (audio_fifo_)
    audio_fifo_->Consume(output_bus_.get(), output_bus_->frames());
  else
    ProvideInput(0, output_bus_.get());

  return noErr;
}

void AUHALStream::ProvideInput(int frame_delay, AudioBus* dest) {
  base::AutoLock auto_lock(source_lock_);
  if (!source_) {
    dest->Zero();
    return;
  }

  // Supply the input data and render the output data.
  source_->OnMoreData(
      dest,
      AudioBuffersState(0,
                        current_hardware_pending_bytes_ +
                            frame_delay * params_.GetBytesPerFrame()));
  dest->Scale(volume_);
}

// AUHAL callback.
OSStatus AUHALStream::InputProc(
    void* user_data,
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* output_time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  // Dispatch to our class method.
  AUHALStream* audio_output =
      static_cast<AUHALStream*>(user_data);
  if (!audio_output)
    return -1;

  return audio_output->Render(
      flags,
      output_time_stamp,
      bus_number,
      number_of_frames,
      io_data);
}

double AUHALStream::GetHardwareLatency() {
  if (!audio_unit_ || device_ == kAudioObjectUnknown) {
    DLOG(WARNING) << "AudioUnit is NULL or device ID is unknown";
    return 0.0;
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(
      audio_unit_,
      kAudioUnitProperty_Latency,
      kAudioUnitScope_Global,
      0,
      &audio_unit_latency_sec,
      &size);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get AudioUnit latency";
    return 0.0;
  }

  // Get output audio device latency.
  static const AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyLatency,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(
      device_,
      &property_address,
      0,
      NULL,
      &size,
      &device_latency_frames);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get audio device latency";
    return 0.0;
  }

  return static_cast<double>((audio_unit_latency_sec *
      output_format_.mSampleRate) + device_latency_frames);
}

double AUHALStream::GetPlayoutLatency(
    const AudioTimeStamp* output_time_stamp) {
  // Ensure mHostTime is valid.
  if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0)
    return 0;

  // Get the delay between the moment getting the callback and the scheduled
  // time stamp that tells when the data is going to be played out.
  UInt64 output_time_ns = AudioConvertHostTimeToNanos(
      output_time_stamp->mHostTime);
  UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

  // Prevent overflow leading to huge delay information; occurs regularly on
  // the bots, probably less so in the wild.
  if (now_ns > output_time_ns)
    return 0;

  double delay_frames = static_cast<double>
      (1e-9 * (output_time_ns - now_ns) * output_format_.mSampleRate);

  return (delay_frames + hardware_latency_frames_);
}

bool AUHALStream::SetStreamFormat(
    AudioStreamBasicDescription* desc,
    int channels,
    UInt32 scope,
    UInt32 element) {
  DCHECK(desc);
  AudioStreamBasicDescription& format = *desc;

  format.mSampleRate = params_.sample_rate();
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked |
      kLinearPCMFormatFlagIsNonInterleaved;
  format.mBytesPerPacket = sizeof(Float32);
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = sizeof(Float32);
  format.mChannelsPerFrame = channels;
  format.mBitsPerChannel = 32;
  format.mReserved = 0;

  OSStatus result = AudioUnitSetProperty(
      audio_unit_,
      kAudioUnitProperty_StreamFormat,
      scope,
      element,
      &format,
      sizeof(format));
  return (result == noErr);
}

bool AUHALStream::ConfigureAUHAL() {
  if (device_ == kAudioObjectUnknown || output_channels_ == 0)
    return false;

  AudioComponentDescription desc = {
      kAudioUnitType_Output,
      kAudioUnitSubType_HALOutput,
      kAudioUnitManufacturer_Apple,
      0,
      0
  };
  AudioComponent comp = AudioComponentFindNext(0, &desc);
  if (!comp)
    return false;

  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioComponentInstanceNew() failed.";
    return false;
  }

  // Enable output as appropriate.
  // See Apple technote for details about the EnableIO property.
  // Note that we use bus 1 for input and bus 0 for output:
  // http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
  UInt32 enable_IO = 1;
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Output,
      0,
      &enable_IO,
      sizeof(enable_IO));
  if (result != noErr)
    return false;

  // Set the device to be used with the AUHAL AudioUnit.
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global,
      0,
      &device_,
      sizeof(AudioDeviceID));
  if (result != noErr)
    return false;

  // Set stream formats.
  // See Apple's tech note for details on the peculiar way that
  // inputs and outputs are handled in the AUHAL concerning scope and bus
  // (element) numbers:
  // http://developer.apple.com/library/mac/#technotes/tn2091/_index.html

  if (!SetStreamFormat(&output_format_,
                       output_channels_,
                       kAudioUnitScope_Input,
                       0)) {
    return false;
  }

  // Set the buffer frame size.
  // WARNING: Setting this value changes the frame size for all output audio
  // units in the current process.  As a result, the AURenderCallback must be
  // able to handle arbitrary buffer sizes and FIFO appropriately.
  UInt32 buffer_size = 0;
  UInt32 property_size = sizeof(buffer_size);
  result = AudioUnitGetProperty(audio_unit_,
                                kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Output,
                                0,
                                &buffer_size,
                                &property_size);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioUnitGetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }

  // Only set the buffer size if we're the only active stream or the buffer size
  // is lower than the current buffer size.
  if (manager_->output_stream_count() == 1 || number_of_frames_ < buffer_size) {
    buffer_size = number_of_frames_;
    result = AudioUnitSetProperty(audio_unit_,
                                  kAudioDevicePropertyBufferFrameSize,
                                  kAudioUnitScope_Output,
                                  0,
                                  &buffer_size,
                                  sizeof(buffer_size));
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "AudioUnitSetProperty("
                                      "kAudioDevicePropertyBufferFrameSize) "
                                      "failed.  Size: " << number_of_frames_;
      return false;
    }
  }

  // Setup callback.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Input,
      0,
      &callback,
      sizeof(callback));
  if (result != noErr)
    return false;

  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioUnitInitialize() failed.";
    return false;
  }

  return true;
}

}  // namespace media
