// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"

#include <CoreAudio/AudioHardware.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/mac/audio_auhal_mac.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Define bounds for for low-latency input and output streams.
static const int kMinimumInputOutputBufferSize = 128;
static const int kMaximumInputOutputBufferSize = 4096;

// Default sample-rate on most Apple hardware.
static const int kFallbackSampleRate = 44100;

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
    selector,
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  UInt32 output_device_id_size = static_cast<UInt32>(sizeof(output_device_id));
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
  return err == kAudioHardwareNoError &&
      output_device_id != kAudioObjectUnknown;
}

// Retrieves information on audio devices, and prepends the default
// device to the list if the list is non-empty.
static void GetAudioDeviceInfo(bool is_input,
                               media::AudioDeviceNames* device_names) {
  // Query the number of total devices.
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                   &property_address,
                                                   0,
                                                   NULL,
                                                   &size);
  if (result || !size)
    return;

  int device_count = size / sizeof(AudioDeviceID);

  // Get the array of device ids for all the devices, which includes both
  // input devices and output devices.
  scoped_ptr<AudioDeviceID, base::FreeDeleter>
      devices(static_cast<AudioDeviceID*>(malloc(size)));
  AudioDeviceID* device_ids = devices.get();
  result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &property_address,
                                      0,
                                      NULL,
                                      &size,
                                      device_ids);
  if (result)
    return;

  // Iterate over all available devices to gather information.
  for (int i = 0; i < device_count; ++i) {
    // Get the number of input or output channels of the device.
    property_address.mScope = is_input ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    property_address.mSelector = kAudioDevicePropertyStreams;
    size = 0;
    result = AudioObjectGetPropertyDataSize(device_ids[i],
                                            &property_address,
                                            0,
                                            NULL,
                                            &size);
    if (result || !size)
      continue;

    // Get device UID.
    CFStringRef uid = NULL;
    size = sizeof(uid);
    property_address.mSelector = kAudioDevicePropertyDeviceUID;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    result = AudioObjectGetPropertyData(device_ids[i],
                                        &property_address,
                                        0,
                                        NULL,
                                        &size,
                                        &uid);
    if (result)
      continue;

    // Get device name.
    CFStringRef name = NULL;
    property_address.mSelector = kAudioObjectPropertyName;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    result = AudioObjectGetPropertyData(device_ids[i],
                                        &property_address,
                                        0,
                                        NULL,
                                        &size,
                                        &name);
    if (result) {
      if (uid)
        CFRelease(uid);
      continue;
    }

    // Store the device name and UID.
    media::AudioDeviceName device_name;
    device_name.device_name = base::SysCFStringRefToUTF8(name);
    device_name.unique_id = base::SysCFStringRefToUTF8(uid);
    device_names->push_back(device_name);

    // We are responsible for releasing the returned CFObject.  See the
    // comment in the AudioHardware.h for constant
    // kAudioDevicePropertyDeviceUID.
    if (uid)
      CFRelease(uid);
    if (name)
      CFRelease(name);
  }

  if (!device_names->empty()) {
    // Prepend the default device to the list since we always want it to be
    // on the top of the list for all platforms. There is no duplicate
    // counting here since the default device has been abstracted out before.
    media::AudioDeviceName name;
    name.device_name = AudioManagerBase::kDefaultDeviceName;
    name.unique_id = AudioManagerBase::kDefaultDeviceId;
    device_names->push_front(name);
  }
}

static AudioDeviceID GetAudioDeviceIdByUId(bool is_input,
                                           const std::string& device_id) {
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  AudioDeviceID audio_device_id = kAudioObjectUnknown;
  UInt32 device_size = sizeof(audio_device_id);
  OSStatus result = -1;

  if (device_id == AudioManagerBase::kDefaultDeviceId || device_id.empty()) {
    // Default Device.
    property_address.mSelector = is_input ?
        kAudioHardwarePropertyDefaultInputDevice :
        kAudioHardwarePropertyDefaultOutputDevice;

    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &device_size,
                                        &audio_device_id);
  } else {
    // Non-default device.
    base::ScopedCFTypeRef<CFStringRef> uid(
        base::SysUTF8ToCFStringRef(device_id));
    AudioValueTranslation value;
    value.mInputData = &uid;
    value.mInputDataSize = sizeof(CFStringRef);
    value.mOutputData = &audio_device_id;
    value.mOutputDataSize = device_size;
    UInt32 translation_size = sizeof(AudioValueTranslation);

    property_address.mSelector = kAudioHardwarePropertyDeviceForUID;
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &translation_size,
                                        &value);
  }

  if (result) {
    OSSTATUS_DLOG(WARNING, result) << "Unable to query device " << device_id
                                   << " for AudioDeviceID";
  }

  return audio_device_id;
}

template <class T>
void StopStreams(std::list<T*>* streams) {
  for (typename std::list<T*>::iterator it = streams->begin();
       it != streams->end();
       ++it) {
    // Stop() is safe to call multiple times, so it doesn't matter if a stream
    // has already been stopped.
    (*it)->Stop();
  }
  streams->clear();
}

class AudioManagerMac::AudioPowerObserver : public base::PowerObserver {
 public:
  AudioPowerObserver()
      : is_suspending_(false),
        is_monitoring_(base::PowerMonitor::Get()) {
    // The PowerMonitor requires signifcant setup (a CFRunLoop and preallocated
    // IO ports) so it's not available under unit tests.  See the OSX impl of
    // base::PowerMonitorDeviceSource for more details.
    if (!is_monitoring_)
      return;
    base::PowerMonitor::Get()->AddObserver(this);
  }

  virtual ~AudioPowerObserver() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!is_monitoring_)
      return;
    base::PowerMonitor::Get()->RemoveObserver(this);
  }

  bool ShouldDeferStreamStart() {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Start() should be deferred if the system is in the middle of a suspend or
    // has recently started the process of resuming.
    return is_suspending_ || base::TimeTicks::Now() < earliest_start_time_;
  }

 private:
  virtual void OnSuspend() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    is_suspending_ = true;
  }

  virtual void OnResume() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    is_suspending_ = false;
    earliest_start_time_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(kStartDelayInSecsForPowerEvents);
  }

  bool is_suspending_;
  const bool is_monitoring_;
  base::TimeTicks earliest_start_time_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioPowerObserver);
};

AudioManagerMac::AudioManagerMac(AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory),
      current_sample_rate_(0),
      current_output_device_(kAudioDeviceUnknown) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // Task must be posted last to avoid races from handing out "this" to the
  // audio thread.  Always PostTask even if we're on the right thread since
  // AudioManager creation is on the startup path and this may be slow.
  GetTaskRunner()->PostTask(FROM_HERE, base::Bind(
      &AudioManagerMac::InitializeOnAudioThread, base::Unretained(this)));
}

AudioManagerMac::~AudioManagerMac() {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    ShutdownOnAudioThread();
  } else {
    // It's safe to post a task here since Shutdown() will wait for all tasks to
    // complete before returning.
    GetTaskRunner()->PostTask(FROM_HERE, base::Bind(
        &AudioManagerMac::ShutdownOnAudioThread, base::Unretained(this)));
  }

  Shutdown();
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

// TODO(xians): There are several places on the OSX specific code which
// could benefit from these helper functions.
bool AudioManagerMac::GetDefaultInputDevice(AudioDeviceID* device) {
  return GetDefaultDevice(device, true);
}

bool AudioManagerMac::GetDefaultOutputDevice(AudioDeviceID* device) {
  return GetDefaultDevice(device, false);
}

bool AudioManagerMac::GetDefaultDevice(AudioDeviceID* device, bool input) {
  CHECK(device);

  // Obtain the current output device selected by the user.
  AudioObjectPropertyAddress pa;
  pa.mSelector = input ? kAudioHardwarePropertyDefaultInputDevice :
      kAudioHardwarePropertyDefaultOutputDevice;
  pa.mScope = kAudioObjectPropertyScopeGlobal;
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size = sizeof(*device);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &pa,
                                               0,
                                               0,
                                               &size,
                                               device);

  if ((result != kAudioHardwareNoError) || (*device == kAudioDeviceUnknown)) {
    DLOG(ERROR) << "Error getting default AudioDevice.";
    return false;
  }

  return true;
}

bool AudioManagerMac::GetDefaultOutputChannels(int* channels) {
  AudioDeviceID device;
  if (!GetDefaultOutputDevice(&device))
    return false;
  return GetDeviceChannels(device, kAudioDevicePropertyScopeOutput, channels);
}

bool AudioManagerMac::GetDeviceChannels(AudioDeviceID device,
                                        AudioObjectPropertyScope scope,
                                        int* channels) {
  CHECK(channels);

  // Get stream configuration.
  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyStreamConfiguration;
  pa.mScope = scope;
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size;
  OSStatus result = AudioObjectGetPropertyDataSize(device, &pa, 0, 0, &size);
  if (result != noErr || !size)
    return false;

  // Allocate storage.
  scoped_ptr<uint8[]> list_storage(new uint8[size]);
  AudioBufferList& buffer_list =
      *reinterpret_cast<AudioBufferList*>(list_storage.get());

  result = AudioObjectGetPropertyData(device, &pa, 0, 0, &size, &buffer_list);
  if (result != noErr)
    return false;

  // Determine number of input channels.
  int channels_per_frame = buffer_list.mNumberBuffers > 0 ?
      buffer_list.mBuffers[0].mNumberChannels : 0;
  if (channels_per_frame == 1 && buffer_list.mNumberBuffers > 1) {
    // Non-interleaved.
    *channels = buffer_list.mNumberBuffers;
  } else {
    // Interleaved.
    *channels = channels_per_frame;
  }

  return true;
}

int AudioManagerMac::HardwareSampleRateForDevice(AudioDeviceID device_id) {
  Float64 nominal_sample_rate;
  UInt32 info_size = sizeof(nominal_sample_rate);

  static const AudioObjectPropertyAddress kNominalSampleRateAddress = {
      kAudioDevicePropertyNominalSampleRate,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyData(device_id,
                                               &kNominalSampleRateAddress,
                                               0,
                                               0,
                                               &info_size,
                                               &nominal_sample_rate);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default sample rate for device: " << device_id;
    return 0;
  }

  return static_cast<int>(nominal_sample_rate);
}

int AudioManagerMac::HardwareSampleRate() {
  // Determine the default output device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  if (!GetDefaultOutputDevice(&device_id))
    return kFallbackSampleRate;

  return HardwareSampleRateForDevice(device_id);
}

void AudioManagerMac::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAudioDeviceInfo(true, device_names);
}

void AudioManagerMac::GetAudioOutputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAudioDeviceInfo(false, device_names);
}

AudioParameters AudioManagerMac::GetInputStreamParameters(
    const std::string& device_id) {
  AudioDeviceID device = GetAudioDeviceIdByUId(true, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid device " << device_id;
    return AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        kFallbackSampleRate, 16, ChooseBufferSize(kFallbackSampleRate));
  }

  int channels = 0;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  if (GetDeviceChannels(device, kAudioDevicePropertyScopeInput, &channels) &&
      channels <= 2) {
    channel_layout = GuessChannelLayout(channels);
  } else {
    DLOG(ERROR) << "Failed to get the device channels, use stereo as default "
                << "for device " << device_id;
  }

  int sample_rate = HardwareSampleRateForDevice(device);
  if (!sample_rate)
    sample_rate = kFallbackSampleRate;

  // Due to the sharing of the input and output buffer sizes, we need to choose
  // the input buffer size based on the output sample rate.  See
  // http://crbug.com/154352.
  const int buffer_size = ChooseBufferSize(sample_rate);

  // TODO(xians): query the native channel layout for the specific device.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, 16, buffer_size);
}

std::string AudioManagerMac::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  AudioDeviceID device = GetAudioDeviceIdByUId(true, input_device_id);
  if (device == kAudioObjectUnknown)
    return std::string();

  UInt32 size = 0;
  AudioObjectPropertyAddress pa = {
    kAudioDevicePropertyRelatedDevices,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyDataSize(device, &pa, 0, 0, &size);
  if (result || !size)
    return std::string();

  int device_count = size / sizeof(AudioDeviceID);
  scoped_ptr<AudioDeviceID, base::FreeDeleter>
      devices(static_cast<AudioDeviceID*>(malloc(size)));
  result = AudioObjectGetPropertyData(
      device, &pa, 0, NULL, &size, devices.get());
  if (result)
    return std::string();

  std::vector<std::string> associated_devices;
  for (int i = 0; i < device_count; ++i) {
    // Get the number of  output channels of the device.
    pa.mSelector = kAudioDevicePropertyStreams;
    size = 0;
    result = AudioObjectGetPropertyDataSize(devices.get()[i],
                                            &pa,
                                            0,
                                            NULL,
                                            &size);
    if (result || !size)
      continue;  // Skip if there aren't any output channels.

    // Get device UID.
    CFStringRef uid = NULL;
    size = sizeof(uid);
    pa.mSelector = kAudioDevicePropertyDeviceUID;
    result = AudioObjectGetPropertyData(devices.get()[i],
                                        &pa,
                                        0,
                                        NULL,
                                        &size,
                                        &uid);
    if (result || !uid)
      continue;

    std::string ret(base::SysCFStringRefToUTF8(uid));
    CFRelease(uid);
    associated_devices.push_back(ret);
  }

  // No matching device found.
  if (associated_devices.empty())
    return std::string();

  // Return the device if there is only one associated device.
  if (associated_devices.size() == 1)
    return associated_devices[0];

  // When there are multiple associated devices, we currently do not have a way
  // to detect if a device (e.g. a digital output device) is actually connected
  // to an endpoint, so we cannot randomly pick a device.
  // We pick the device iff the associated device is the default output device.
  const std::string default_device = GetDefaultOutputDeviceID();
  for (std::vector<std::string>::const_iterator iter =
           associated_devices.begin();
       iter != associated_devices.end(); ++iter) {
    if (default_device == *iter)
      return *iter;
  }

  // Failed to figure out which is the matching device, return an emtpy string.
  return std::string();
}

AudioOutputStream* AudioManagerMac::MakeLinearOutputStream(
    const AudioParameters& params) {
  return MakeLowLatencyOutputStream(params, std::string());
}

AudioOutputStream* AudioManagerMac::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  AudioDeviceID device = GetAudioDeviceIdByUId(false, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Failed to open output device: " << device_id;
    return NULL;
  }

  // Lazily create the audio device listener on the first stream creation.
  if (!output_device_listener_) {
    // NOTE: Use BindToCurrentLoop() to ensure the callback is always PostTask'd
    // even if OSX calls us on the right thread.  Some CoreAudio drivers will
    // fire the callbacks during stream creation, leading to re-entrancy issues
    // otherwise.  See http://crbug.com/349604
    output_device_listener_.reset(
        new AudioDeviceListenerMac(BindToCurrentLoop(base::Bind(
            &AudioManagerMac::HandleDeviceChanges, base::Unretained(this)))));
    // Only set the current output device for the default device.
    if (device_id == AudioManagerBase::kDefaultDeviceId || device_id.empty())
      current_output_device_ = device;
    // Just use the current sample rate since we don't allow non-native sample
    // rates on OSX.
    current_sample_rate_ = params.sample_rate();
  }

  AudioOutputStream* stream = new AUHALStream(this, params, device);
  output_streams_.push_back(stream);
  return stream;
}

std::string AudioManagerMac::GetDefaultOutputDeviceID() {
  AudioDeviceID device_id = kAudioObjectUnknown;
  if (!GetDefaultOutputDevice(&device_id))
    return std::string();

  const AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyDeviceUID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  CFStringRef device_uid = NULL;
  UInt32 size = sizeof(device_uid);
  OSStatus status = AudioObjectGetPropertyData(device_id,
                                               &property_address,
                                               0,
                                               NULL,
                                               &size,
                                               &device_uid);
  if (status != kAudioHardwareNoError || !device_uid)
    return std::string();

  std::string ret(base::SysCFStringRefToUTF8(device_uid));
  CFRelease(device_uid);

  return ret;
}

AudioInputStream* AudioManagerMac::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  AudioInputStream* stream = new PCMQueueInAudioInputStream(this, params);
  input_streams_.push_back(stream);
  return stream;
}

AudioInputStream* AudioManagerMac::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // Gets the AudioDeviceID that refers to the AudioInputDevice with the device
  // unique id. This AudioDeviceID is used to set the device for Audio Unit.
  AudioDeviceID audio_device_id = GetAudioDeviceIdByUId(true, device_id);
  AudioInputStream* stream = NULL;
  if (audio_device_id != kAudioObjectUnknown) {
    // AUAudioInputStream needs to be fed the preferred audio output parameters
    // of the matching device so that the buffer size of both input and output
    // can be matched.  See constructor of AUAudioInputStream for more.
    const std::string associated_output_device(
        GetAssociatedOutputDeviceID(device_id));
    const AudioParameters output_params =
        GetPreferredOutputStreamParameters(
            associated_output_device.empty() ?
                AudioManagerBase::kDefaultDeviceId : associated_output_device,
            params);
    stream = new AUAudioInputStream(this, params, output_params,
        audio_device_id);
    input_streams_.push_back(stream);
  }

  return stream;
}

AudioParameters AudioManagerMac::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  const AudioDeviceID device = GetAudioDeviceIdByUId(false, output_device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid output device " << output_device_id;
    return input_params.IsValid() ? input_params : AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        kFallbackSampleRate, 16, ChooseBufferSize(kFallbackSampleRate));
  }

  const bool has_valid_input_params = input_params.IsValid();
  const int hardware_sample_rate = HardwareSampleRateForDevice(device);

  // Allow pass through buffer sizes.  If concurrent input and output streams
  // exist, they will use the smallest buffer size amongst them.  As such, each
  // stream must be able to FIFO requests appropriately when this happens.
  int buffer_size = ChooseBufferSize(hardware_sample_rate);
  if (has_valid_input_params) {
    buffer_size =
        std::min(kMaximumInputOutputBufferSize,
                 std::max(input_params.frames_per_buffer(), buffer_size));
  }

  int hardware_channels;
  if (!GetDeviceChannels(device, kAudioDevicePropertyScopeOutput,
                         &hardware_channels)) {
    hardware_channels = 2;
  }

  // Use the input channel count and channel layout if possible.  Let OSX take
  // care of remapping the channels; this lets user specified channel layouts
  // work correctly.
  int output_channels = input_params.channels();
  ChannelLayout channel_layout = input_params.channel_layout();
  if (!has_valid_input_params || output_channels > hardware_channels) {
    output_channels = hardware_channels;
    channel_layout = GuessChannelLayout(output_channels);
    if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED)
      channel_layout = CHANNEL_LAYOUT_DISCRETE;
  }

  const int input_channels =
      has_valid_input_params ? input_params.input_channels() : 0;
  if (input_channels > 0) {
    // TODO(xians): given the limitations of the AudioOutputStream
    // back-ends used with synchronized I/O, we hard-code to stereo.
    // Specifically, this is a limitation of AudioSynchronizedStream which
    // can be removed as part of the work to consolidate these back-ends.
    channel_layout = CHANNEL_LAYOUT_STEREO;
  }

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, output_channels,
      input_channels, hardware_sample_rate, 16, buffer_size,
      AudioParameters::NO_EFFECTS);
}

void AudioManagerMac::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  power_observer_.reset(new AudioPowerObserver());
}

void AudioManagerMac::ShutdownOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_device_listener_.reset();
  power_observer_.reset();

  // Since CoreAudio calls have to run on the UI thread and browser shutdown
  // doesn't wait for outstanding tasks to complete, we may have input/output
  // streams still running at shutdown.
  //
  // To avoid calls into destructed classes, we need to stop the OS callbacks
  // by stopping the streams.  Note: The streams are leaked since process
  // destruction is imminent.
  //
  // See http://crbug.com/354139 for crash details.
  StopStreams(&input_streams_);
  StopStreams(&output_streams_);
}

void AudioManagerMac::HandleDeviceChanges() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const int new_sample_rate = HardwareSampleRate();
  AudioDeviceID new_output_device;
  GetDefaultOutputDevice(&new_output_device);

  if (current_sample_rate_ == new_sample_rate &&
      current_output_device_ == new_output_device)
    return;

  current_sample_rate_ = new_sample_rate;
  current_output_device_ = new_output_device;
  NotifyAllOutputDeviceChangeListeners();
}

int AudioManagerMac::ChooseBufferSize(int output_sample_rate) {
  int buffer_size = kMinimumInputOutputBufferSize;
  const int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  } else if (output_sample_rate > 48000) {
    // The default buffer size is too small for higher sample rates and may lead
    // to glitching.  Adjust upwards by multiples of the default size.
    if (output_sample_rate <= 96000)
      buffer_size = 2 * kMinimumInputOutputBufferSize;
    else if (output_sample_rate <= 192000)
      buffer_size = 4 * kMinimumInputOutputBufferSize;
  }

  return buffer_size;
}

bool AudioManagerMac::ShouldDeferStreamStart() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->ShouldDeferStreamStart();
}

void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  output_streams_.remove(stream);
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerMac::ReleaseInputStream(AudioInputStream* stream) {
  input_streams_.remove(stream);
  AudioManagerBase::ReleaseInputStream(stream);
}

AudioManager* CreateAudioManager(AudioLogFactory* audio_log_factory) {
  return new AudioManagerMac(audio_log_factory);
}

}  // namespace media
