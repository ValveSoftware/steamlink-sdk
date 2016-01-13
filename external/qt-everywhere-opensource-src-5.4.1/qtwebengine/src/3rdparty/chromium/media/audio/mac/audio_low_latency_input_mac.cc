// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_low_latency_input_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_bus.h"
#include "media/base/data_buffer.h"

namespace media {

static std::ostream& operator<<(std::ostream& os,
                                const AudioStreamBasicDescription& format) {
  os << "sample rate       : " << format.mSampleRate << std::endl
     << "format ID         : " << format.mFormatID << std::endl
     << "format flags      : " << format.mFormatFlags << std::endl
     << "bytes per packet  : " << format.mBytesPerPacket << std::endl
     << "frames per packet : " << format.mFramesPerPacket << std::endl
     << "bytes per frame   : " << format.mBytesPerFrame << std::endl
     << "channels per frame: " << format.mChannelsPerFrame << std::endl
     << "bits per channel  : " << format.mBitsPerChannel;
  return os;
}

// See "Technical Note TN2091 - Device input using the HAL Output Audio Unit"
// http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
// for more details and background regarding this implementation.

AUAudioInputStream::AUAudioInputStream(AudioManagerMac* manager,
                                       const AudioParameters& input_params,
                                       const AudioParameters& output_params,
                                       AudioDeviceID audio_device_id)
    : manager_(manager),
      sink_(NULL),
      audio_unit_(0),
      input_device_id_(audio_device_id),
      started_(false),
      hardware_latency_frames_(0),
      fifo_delay_bytes_(0),
      number_of_channels_in_frame_(0),
      audio_bus_(media::AudioBus::Create(input_params)) {
  DCHECK(manager_);

  // Set up the desired (output) format specified by the client.
  format_.mSampleRate = input_params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = input_params.bits_per_sample();
  format_.mChannelsPerFrame = input_params.channels();
  format_.mFramesPerPacket = 1;  // uncompressed audio
  format_.mBytesPerPacket = (format_.mBitsPerChannel *
                             input_params.channels()) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
  format_.mReserved = 0;

  DVLOG(1) << "Desired ouput format: " << format_;

  // Set number of sample frames per callback used by the internal audio layer.
  // An internal FIFO is then utilized to adapt the internal size to the size
  // requested by the client.
  number_of_frames_ = output_params.frames_per_buffer();
  DVLOG(1) << "Size of data buffer in frames : " << number_of_frames_;

  // Derive size (in bytes) of the buffers that we will render to.
  UInt32 data_byte_size = number_of_frames_ * format_.mBytesPerFrame;
  DVLOG(1) << "Size of data buffer in bytes : " << data_byte_size;

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer.
  audio_data_buffer_.reset(new uint8[data_byte_size]);
  audio_buffer_list_.mNumberBuffers = 1;

  AudioBuffer* audio_buffer = audio_buffer_list_.mBuffers;
  audio_buffer->mNumberChannels = input_params.channels();
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = audio_data_buffer_.get();

  // Set up an internal FIFO buffer that will accumulate recorded audio frames
  // until a requested size is ready to be sent to the client.
  // It is not possible to ask for less than |kAudioFramesPerCallback| number of
  // audio frames.
  size_t requested_size_frames =
      input_params.GetBytesPerBuffer() / format_.mBytesPerPacket;
  if (requested_size_frames < number_of_frames_) {
    // For devices that only support a low sample rate like 8kHz, we adjust the
    // buffer size to match number_of_frames_.  The value of number_of_frames_
    // in this case has not been calculated based on hardware settings but
    // rather our hardcoded defaults (see ChooseBufferSize).
    requested_size_frames = number_of_frames_;
  }

  requested_size_bytes_ = requested_size_frames * format_.mBytesPerFrame;
  DVLOG(1) << "Requested buffer size in bytes : " << requested_size_bytes_;
  DVLOG_IF(0, requested_size_frames > number_of_frames_) << "FIFO is used";

  const int number_of_bytes = number_of_frames_ * format_.mBytesPerFrame;
  fifo_delay_bytes_ = requested_size_bytes_ - number_of_bytes;

  // Allocate some extra memory to avoid memory reallocations.
  // Ensure that the size is an even multiple of |number_of_frames_ and
  // larger than |requested_size_frames|.
  // Example: number_of_frames_=128, requested_size_frames=480 =>
  // allocated space equals 4*128=512 audio frames
  const int max_forward_capacity = number_of_bytes *
      ((requested_size_frames / number_of_frames_) + 1);
  fifo_.reset(new media::SeekableBuffer(0, max_forward_capacity));

  data_ = new media::DataBuffer(requested_size_bytes_);
}

AUAudioInputStream::~AUAudioInputStream() {}

// Obtain and open the AUHAL AudioOutputUnit for recording.
bool AUAudioInputStream::Open() {
  // Verify that we are not already opened.
  if (audio_unit_)
    return false;

  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return false;
  }

  // Start by obtaining an AudioOuputUnit using an AUHAL component description.

  Component comp;
  ComponentDescription desc;

  // Description for the Audio Unit we want to use (AUHAL in this case).
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_HALOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  comp = FindNextComponent(0, &desc);
  DCHECK(comp);

  // Get access to the service provided by the specified Audio Unit.
  OSStatus result = OpenAComponent(comp, &audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // Enable IO on the input scope of the Audio Unit.

  // After creating the AUHAL object, we must enable IO on the input scope
  // of the Audio Unit to obtain the device input. Input must be explicitly
  // enabled with the kAudioOutputUnitProperty_EnableIO property on Element 1
  // of the AUHAL. Beacause the AUHAL can be used for both input and output,
  // we must also disable IO on the output scope.

  UInt32 enableIO = 1;

  // Enable input on the AUHAL.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                1,          // input element 1
                                &enableIO,  // enable
                                sizeof(enableIO));
  if (result) {
    HandleError(result);
    return false;
  }

  // Disable output on the AUHAL.
  enableIO = 0;
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                0,          // output element 0
                                &enableIO,  // disable
                                sizeof(enableIO));
  if (result) {
    HandleError(result);
    return false;
  }

  // Next, set the audio device to be the Audio Unit's current device.
  // Note that, devices can only be set to the AUHAL after enabling IO.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global,
                                0,
                                &input_device_id_,
                                sizeof(input_device_id_));
  if (result) {
    HandleError(result);
    return false;
  }

  // Register the input procedure for the AUHAL.
  // This procedure will be called when the AUHAL has received new data
  // from the input device.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_SetInputCallback,
                                kAudioUnitScope_Global,
                                0,
                                &callback,
                                sizeof(callback));
  if (result) {
    HandleError(result);
    return false;
  }

  // Set up the the desired (output) format.
  // For obtaining input from a device, the device format is always expressed
  // on the output scope of the AUHAL's Element 1.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output,
                                1,
                                &format_,
                                sizeof(format_));
  if (result) {
    HandleError(result);
    return false;
  }

  // Set the desired number of frames in the IO buffer (output scope).
  // WARNING: Setting this value changes the frame size for all input audio
  // units in the current process.  As a result, the AURenderCallback must be
  // able to handle arbitrary buffer sizes and FIFO appropriately.
  UInt32 buffer_size = 0;
  UInt32 property_size = sizeof(buffer_size);
  result = AudioUnitGetProperty(audio_unit_,
                                kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Output,
                                1,
                                &buffer_size,
                                &property_size);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Only set the buffer size if we're the only active stream or the buffer size
  // is lower than the current buffer size.
  if (manager_->input_stream_count() == 1 || number_of_frames_ < buffer_size) {
    buffer_size = number_of_frames_;
    result = AudioUnitSetProperty(audio_unit_,
                                  kAudioDevicePropertyBufferFrameSize,
                                  kAudioUnitScope_Output,
                                  1,
                                  &buffer_size,
                                  sizeof(buffer_size));
    if (result != noErr) {
      HandleError(result);
      return false;
    }
  }

  // Finally, initialize the audio unit and ensure that it is ready to render.
  // Allocates memory according to the maximum number of audio frames
  // it can produce in response to a single render call.
  result = AudioUnitInitialize(audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // The hardware latency is fixed and will not change during the call.
  hardware_latency_frames_ = GetHardwareLatency();

  // The master channel is 0, Left and right are channels 1 and 2.
  // And the master channel is not counted in |number_of_channels_in_frame_|.
  number_of_channels_in_frame_ = GetNumberOfChannelsFromStream();

  return true;
}

void AUAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DLOG_IF(ERROR, !audio_unit_) << "Open() has not been called successfully";
  if (started_ || !audio_unit_)
    return;

  // Check if we should defer Start() for http://crbug.com/160920.
  if (manager_->ShouldDeferStreamStart()) {
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(base::Bind(
        &AUAudioInputStream::Start, base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE,
        deferred_start_cb_.callback(),
        base::TimeDelta::FromSeconds(
            AudioManagerMac::kStartDelayInSecsForPowerEvents));
    return;
  }

  sink_ = callback;
  StartAgc();
  OSStatus result = AudioOutputUnitStart(audio_unit_);
  if (result == noErr) {
    started_ = true;
  }
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "Failed to start acquiring data";
}

void AUAudioInputStream::Stop() {
  if (!started_)
    return;
  StopAgc();
  OSStatus result = AudioOutputUnitStop(audio_unit_);
  DCHECK_EQ(result, noErr);
  started_ = false;
  sink_ = NULL;

  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "Failed to stop acquiring data";
}

void AUAudioInputStream::Close() {
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  if (started_) {
    Stop();
  }
  if (audio_unit_) {
    // Deallocate the audio unit’s resources.
    AudioUnitUninitialize(audio_unit_);

    // Terminates our connection to the AUHAL component.
    CloseComponent(audio_unit_);
    audio_unit_ = 0;
  }

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double AUAudioInputStream::GetMaxVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  // Query if any of the master, left or right channels has volume control.
  for (int i = 0; i <= number_of_channels_in_frame_; ++i) {
    // If the volume is settable, the  valid volume range is [0.0, 1.0].
    if (IsVolumeSettableOnChannel(i))
      return 1.0;
  }

  // Volume control is not available for the audio stream.
  return 0.0;
}

void AUAudioInputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  DCHECK_GE(volume, 0.0);
  DCHECK_LE(volume, 1.0);

  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return;
  }

  Float32 volume_float32 = static_cast<Float32>(volume);
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  // Try to set the volume for master volume channel.
  if (IsVolumeSettableOnChannel(kAudioObjectPropertyElementMaster)) {
    OSStatus result = AudioObjectSetPropertyData(input_device_id_,
                                                 &property_address,
                                                 0,
                                                 NULL,
                                                 sizeof(volume_float32),
                                                 &volume_float32);
    if (result != noErr) {
      DLOG(WARNING) << "Failed to set volume to " << volume_float32;
    }
    return;
  }

  // There is no master volume control, try to set volume for each channel.
  int successful_channels = 0;
  for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
    property_address.mElement = static_cast<UInt32>(i);
    if (IsVolumeSettableOnChannel(i)) {
      OSStatus result = AudioObjectSetPropertyData(input_device_id_,
                                                   &property_address,
                                                   0,
                                                   NULL,
                                                   sizeof(volume_float32),
                                                   &volume_float32);
      if (result == noErr)
        ++successful_channels;
    }
  }

  DLOG_IF(WARNING, successful_channels == 0)
      << "Failed to set volume to " << volume_float32;

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double AUAudioInputStream::GetVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown){
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  if (AudioObjectHasProperty(input_device_id_, &property_address)) {
    // The device supports master volume control, get the volume from the
    // master channel.
    Float32 volume_float32 = 0.0;
    UInt32 size = sizeof(volume_float32);
    OSStatus result = AudioObjectGetPropertyData(input_device_id_,
                                                 &property_address,
                                                 0,
                                                 NULL,
                                                 &size,
                                                 &volume_float32);
    if (result == noErr)
      return static_cast<double>(volume_float32);
  } else {
    // There is no master volume control, try to get the average volume of
    // all the channels.
    Float32 volume_float32 = 0.0;
    int successful_channels = 0;
    for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
      property_address.mElement = static_cast<UInt32>(i);
      if (AudioObjectHasProperty(input_device_id_, &property_address)) {
        Float32 channel_volume = 0;
        UInt32 size = sizeof(channel_volume);
        OSStatus result = AudioObjectGetPropertyData(input_device_id_,
                                                     &property_address,
                                                     0,
                                                     NULL,
                                                     &size,
                                                     &channel_volume);
        if (result == noErr) {
          volume_float32 += channel_volume;
          ++successful_channels;
        }
      }
    }

    // Get the average volume of the channels.
    if (successful_channels != 0)
      return static_cast<double>(volume_float32 / successful_channels);
  }

  DLOG(WARNING) << "Failed to get volume";
  return 0.0;
}

// AUHAL AudioDeviceOutput unit callback
OSStatus AUAudioInputStream::InputProc(void* user_data,
                                       AudioUnitRenderActionFlags* flags,
                                       const AudioTimeStamp* time_stamp,
                                       UInt32 bus_number,
                                       UInt32 number_of_frames,
                                       AudioBufferList* io_data) {
  // Verify that the correct bus is used (Input bus/Element 1)
  DCHECK_EQ(bus_number, static_cast<UInt32>(1));
  AUAudioInputStream* audio_input =
      reinterpret_cast<AUAudioInputStream*>(user_data);
  DCHECK(audio_input);
  if (!audio_input)
    return kAudioUnitErr_InvalidElement;

  // Receive audio from the AUHAL from the output scope of the Audio Unit.
  OSStatus result = AudioUnitRender(audio_input->audio_unit(),
                                    flags,
                                    time_stamp,
                                    bus_number,
                                    number_of_frames,
                                    audio_input->audio_buffer_list());
  if (result)
    return result;

  // Deliver recorded data to the consumer as a callback.
  return audio_input->Provide(number_of_frames,
                              audio_input->audio_buffer_list(),
                              time_stamp);
}

OSStatus AUAudioInputStream::Provide(UInt32 number_of_frames,
                                     AudioBufferList* io_data,
                                     const AudioTimeStamp* time_stamp) {
  // Update the capture latency.
  double capture_latency_frames = GetCaptureLatency(time_stamp);

  // The AGC volume level is updated once every second on a separate thread.
  // Note that, |volume| is also updated each time SetVolume() is called
  // through IPC by the render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8* audio_data = reinterpret_cast<uint8*>(buffer.mData);
  uint32 capture_delay_bytes = static_cast<uint32>
      ((capture_latency_frames + 0.5) * format_.mBytesPerFrame);
  // Account for the extra delay added by the FIFO.
  capture_delay_bytes += fifo_delay_bytes_;
  DCHECK(audio_data);
  if (!audio_data)
    return kAudioUnitErr_InvalidElement;

  // Accumulate captured audio in FIFO until we can match the output size
  // requested by the client.
  fifo_->Append(audio_data, buffer.mDataByteSize);

  // Deliver recorded data to the client as soon as the FIFO contains a
  // sufficient amount.
  if (fifo_->forward_bytes() >= requested_size_bytes_) {
    // Read from FIFO into temporary data buffer.
    fifo_->Read(data_->writable_data(), requested_size_bytes_);

    // Copy captured (and interleaved) data into deinterleaved audio bus.
    audio_bus_->FromInterleaved(
        data_->data(), audio_bus_->frames(), format_.mBitsPerChannel / 8);

    // Deliver data packet, delay estimation and volume level to the user.
    sink_->OnData(
        this, audio_bus_.get(), capture_delay_bytes, normalized_volume);
  }

  return noErr;
}

int AUAudioInputStream::HardwareSampleRate() {
  // Determine the default input device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  UInt32 info_size = sizeof(device_id);

  AudioObjectPropertyAddress default_input_device_address = {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &default_input_device_address,
                                               0,
                                               0,
                                               &info_size,
                                               &device_id);
  if (result != noErr)
    return 0.0;

  Float64 nominal_sample_rate;
  info_size = sizeof(nominal_sample_rate);

  AudioObjectPropertyAddress nominal_sample_rate_address = {
    kAudioDevicePropertyNominalSampleRate,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  result = AudioObjectGetPropertyData(device_id,
                                      &nominal_sample_rate_address,
                                      0,
                                      0,
                                      &info_size,
                                      &nominal_sample_rate);
  if (result != noErr)
    return 0.0;

  return static_cast<int>(nominal_sample_rate);
}

double AUAudioInputStream::GetHardwareLatency() {
  if (!audio_unit_ || input_device_id_ == kAudioObjectUnknown) {
    DLOG(WARNING) << "Audio unit object is NULL or device ID is unknown";
    return 0.0;
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(audio_unit_,
                                         kAudioUnitProperty_Latency,
                                         kAudioUnitScope_Global,
                                         0,
                                         &audio_unit_latency_sec,
                                         &size);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio unit latency";

  // Get input audio device latency.
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyLatency,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };
  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(input_device_id_,
                                      &property_address,
                                      0,
                                      NULL,
                                      &size,
                                      &device_latency_frames);
  DLOG_IF(WARNING, result != noErr) << "Could not get audio device latency.";

  return static_cast<double>((audio_unit_latency_sec *
      format_.mSampleRate) + device_latency_frames);
}

double AUAudioInputStream::GetCaptureLatency(
    const AudioTimeStamp* input_time_stamp) {
  // Get the delay between between the actual recording instant and the time
  // when the data packet is provided as a callback.
  UInt64 capture_time_ns = AudioConvertHostTimeToNanos(
      input_time_stamp->mHostTime);
  UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
  double delay_frames = static_cast<double>
      (1e-9 * (now_ns - capture_time_ns) * format_.mSampleRate);

  // Total latency is composed by the dynamic latency and the fixed
  // hardware latency.
  return (delay_frames + hardware_latency_frames_);
}

int AUAudioInputStream::GetNumberOfChannelsFromStream() {
  // Get the stream format, to be able to read the number of channels.
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyStreamFormat,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };
  AudioStreamBasicDescription stream_format;
  UInt32 size = sizeof(stream_format);
  OSStatus result = AudioObjectGetPropertyData(input_device_id_,
                                               &property_address,
                                               0,
                                               NULL,
                                               &size,
                                               &stream_format);
  if (result != noErr) {
    DLOG(WARNING) << "Could not get stream format";
    return 0;
  }

  return static_cast<int>(stream_format.mChannelsPerFrame);
}

void AUAudioInputStream::HandleError(OSStatus err) {
  NOTREACHED() << "error " << GetMacOSStatusErrorString(err)
               << " (" << err << ")";
  if (sink_)
    sink_->OnError(this);
}

bool AUAudioInputStream::IsVolumeSettableOnChannel(int channel) {
  Boolean is_settable = false;
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    static_cast<UInt32>(channel)
  };
  OSStatus result = AudioObjectIsPropertySettable(input_device_id_,
                                                  &property_address,
                                                  &is_settable);
  return (result == noErr) ? is_settable : false;
}

}  // namespace media
