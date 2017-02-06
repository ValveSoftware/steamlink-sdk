// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/mac/audio_auhal_mac.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/base/audio_parameters.h"
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

// Helper method to construct AudioObjectPropertyAddress structure given
// property selector and scope. The property element is always set to
// kAudioObjectPropertyElementMaster.
static AudioObjectPropertyAddress GetAudioObjectPropertyAddress(
    AudioObjectPropertySelector selector,
    bool is_input) {
  AudioObjectPropertyScope scope = is_input ? kAudioObjectPropertyScopeInput
                                            : kAudioObjectPropertyScopeOutput;
  AudioObjectPropertyAddress property_address = {
      selector, scope, kAudioObjectPropertyElementMaster};
  return property_address;
}

// Get IO buffer size range from HAL given device id and scope.
static OSStatus GetIOBufferFrameSizeRange(AudioDeviceID device_id,
                                          bool is_input,
                                          UInt32* minimum,
                                          UInt32* maximum) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioObjectPropertyAddress address = GetAudioObjectPropertyAddress(
      kAudioDevicePropertyBufferFrameSizeRange, is_input);
  AudioValueRange range = {0, 0};
  UInt32 data_size = sizeof(AudioValueRange);
  OSStatus error = AudioObjectGetPropertyData(device_id, &address, 0, NULL,
                                              &data_size, &range);
  if (error != noErr) {
    OSSTATUS_DLOG(WARNING, error)
        << "Failed to query IO buffer size range for device: " << std::hex
        << device_id;
  } else {
    *minimum = range.mMinimum;
    *maximum = range.mMaximum;
  }
  return error;
}

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
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

static std::string GetAudioDeviceNameFromDeviceId(AudioDeviceID device_id,
                                                  bool is_input) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CFStringRef device_name = nullptr;
  UInt32 data_size = sizeof(device_name);
  AudioObjectPropertyAddress property_address = GetAudioObjectPropertyAddress(
      kAudioDevicePropertyDeviceNameCFString, is_input);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0, nullptr, &data_size, &device_name);
  std::string device;
  if (result == noErr) {
    device = base::SysCFStringRefToUTF8(device_name);
    CFRelease(device_name);
  }
  return device;
}

// Retrieves information on audio devices, and prepends the default
// device to the list if the list is non-empty.
static void GetAudioDeviceInfo(bool is_input,
                               media::AudioDeviceNames* device_names) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
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
  std::unique_ptr<AudioDeviceID, base::FreeDeleter> devices(
      static_cast<AudioDeviceID*>(malloc(size)));
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
    device_names->push_front(media::AudioDeviceName::CreateDefault());
  }
}

static AudioDeviceID GetAudioDeviceIdByUId(bool is_input,
                                           const std::string& device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  AudioDeviceID audio_device_id = kAudioObjectUnknown;
  UInt32 device_size = sizeof(audio_device_id);
  OSStatus result = -1;

  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
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

static bool GetDefaultDevice(AudioDeviceID* device, bool input) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(device);

  // Obtain the AudioDeviceID of the default input or output AudioDevice.
  AudioObjectPropertyAddress pa;
  pa.mSelector = input ? kAudioHardwarePropertyDefaultInputDevice
                       : kAudioHardwarePropertyDefaultOutputDevice;
  pa.mScope = kAudioObjectPropertyScopeGlobal;
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size = sizeof(*device);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0,
                                               0, &size, device);
  if ((result != kAudioHardwareNoError) || (*device == kAudioDeviceUnknown)) {
    DLOG(ERROR) << "Error getting default AudioDevice.";
    return false;
  }
  return true;
}

static bool GetDefaultOutputDevice(AudioDeviceID* device) {
  return GetDefaultDevice(device, false);
}

class AudioManagerMac::AudioPowerObserver : public base::PowerObserver {
 public:
  AudioPowerObserver()
      : is_suspending_(false),
        is_monitoring_(base::PowerMonitor::Get()),
        num_resume_notifications_(0) {
    // The PowerMonitor requires significant setup (a CFRunLoop and preallocated
    // IO ports) so it's not available under unit tests.  See the OSX impl of
    // base::PowerMonitorDeviceSource for more details.
    if (!is_monitoring_)
      return;
    base::PowerMonitor::Get()->AddObserver(this);
  }

  ~AudioPowerObserver() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!is_monitoring_)
      return;
    base::PowerMonitor::Get()->RemoveObserver(this);
  }

  bool IsSuspending() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_suspending_;
  }

  size_t num_resume_notifications() const { return num_resume_notifications_; }

  bool ShouldDeferStreamStart() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Start() should be deferred if the system is in the middle of a suspend or
    // has recently started the process of resuming.
    return is_suspending_ || base::TimeTicks::Now() < earliest_start_time_;
  }

  bool IsOnBatteryPower() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return base::PowerMonitor::Get()->IsOnBatteryPower();
  }

 private:
  void OnSuspend() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "OnSuspend";
    is_suspending_ = true;
  }

  void OnResume() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "OnResume";
    ++num_resume_notifications_;
    is_suspending_ = false;
    earliest_start_time_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(kStartDelayInSecsForPowerEvents);
  }

  bool is_suspending_;
  const bool is_monitoring_;
  base::TimeTicks earliest_start_time_;
  base::ThreadChecker thread_checker_;
  size_t num_resume_notifications_;

  DISALLOW_COPY_AND_ASSIGN(AudioPowerObserver);
};

AudioManagerMac::AudioManagerMac(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(task_runner),
                       std::move(worker_task_runner),
                       audio_log_factory),
      current_sample_rate_(0),
      current_output_device_(kAudioDeviceUnknown),
      in_shutdown_(false) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // Task must be posted last to avoid races from handing out "this" to the
  // audio thread.  Always PostTask even if we're on the right thread since
  // AudioManager creation is on the startup path and this may be slow.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&AudioManagerMac::InitializeOnAudioThread,
                            base::Unretained(this)));
}

AudioManagerMac::~AudioManagerMac() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  // We are now in shutdown mode. This flag disables MaybeChangeBufferSize()
  // and IncreaseIOBufferSizeIfPossible() which both touches native Core Audio
  // APIs and they can fail and disrupt tests during shutdown.
  in_shutdown_ = true;
  // We have seen cases where active input audio is not closed down properly
  // at browser shutdown. AudioInputController::Close() is called but tasks
  // in AudioInputController::DoClose() are not executed. Hence, input streams
  // might remain even at this late state. |low_latency_input_streams_| will be
  // modified during the call to stream->Close(), so we can't iterate over it
  // here.  Instead iterate over a copy.
  // TODO(henrika): figure out the real cause why streams are not closed
  // properly by the AIC for all cases and then remove this loop.
  auto low_latency_input_streams_copy = low_latency_input_streams_;
  for (auto* stream : low_latency_input_streams_copy) {
    LOG(WARNING) << "Closing existing audio input stream at destruction";
    // Prevents active Core Audio callbacks to use possibly invalid objects
    // in its OnData() callback.
    stream->Stop();
    // Avoids hitting CHECK in dtor of AudioManagerBase.
    stream->Close();
  }
  Shutdown();
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

// static
bool AudioManagerMac::GetDeviceChannels(AudioDeviceID device,
                                        AudioObjectPropertyScope scope,
                                        int* channels) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(channels);
  const bool is_input = (scope == kAudioDevicePropertyScopeInput);
  DVLOG(1) << "GetDeviceChannels(id=0x" << std::hex << device
           << ", is_input=" << is_input << ")";

  // Get the stream configuration of the device in an AudioBufferList (with the
  // buffer pointers set to NULL) which describes the list of streams and the
  // number of channels in each stream.
  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyStreamConfiguration;
  pa.mScope = scope;
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size;
  OSStatus result = AudioObjectGetPropertyDataSize(device, &pa, 0, 0, &size);
  if (result != noErr || !size)
    return false;

  std::unique_ptr<uint8_t[]> list_storage(new uint8_t[size]);
  AudioBufferList& buffer_list =
      *reinterpret_cast<AudioBufferList*>(list_storage.get());

  result = AudioObjectGetPropertyData(device, &pa, 0, 0, &size, &buffer_list);
  if (result != noErr)
    return false;

  // Determine number of channels based on the AudioBufferList.
  // |mNumberBuffers] is the  number of interleaved channels in the buffer.
  // If the number is 1, the buffer is noninterleaved.
  // TODO(henrika): add UMA stats to track utilized hardware configurations.
  int num_channels = 0;
  for (UInt32 i = 0; i < buffer_list.mNumberBuffers; ++i) {
    num_channels += buffer_list.mBuffers[i].mNumberChannels;
  }
  *channels = num_channels;
  DVLOG(1) << "#channels: " << *channels;
  return true;
}

// static
int AudioManagerMac::HardwareSampleRateForDevice(AudioDeviceID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
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

// static
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
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceID device = GetAudioDeviceIdByUId(true, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid device " << device_id;
    return AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        kFallbackSampleRate, 16, ChooseBufferSize(true, kFallbackSampleRate));
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
  const int buffer_size = ChooseBufferSize(true, sample_rate);

  // TODO(xians): query the native channel layout for the specific device.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, 16, buffer_size);
}

std::string AudioManagerMac::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
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
  std::unique_ptr<AudioDeviceID, base::FreeDeleter> devices(
      static_cast<AudioDeviceID*>(malloc(size)));
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

  // Failed to figure out which is the matching device, return an empty string.
  return std::string();
}

AudioOutputStream* AudioManagerMac::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return MakeLowLatencyOutputStream(params, std::string(), log_callback);
}

AudioOutputStream* AudioManagerMac::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool device_listener_first_init = false;
  // Lazily create the audio device listener on the first stream creation,
  // even if getting an audio device fails. Otherwise, if we have 0 audio
  // devices, the listener will never be initialized, and new valid devices
  // will never be detected.
  if (!output_device_listener_) {
    // NOTE: Use BindToCurrentLoop() to ensure the callback is always PostTask'd
    // even if OSX calls us on the right thread.  Some CoreAudio drivers will
    // fire the callbacks during stream creation, leading to re-entrancy issues
    // otherwise.  See http://crbug.com/349604
    output_device_listener_.reset(
        new AudioDeviceListenerMac(BindToCurrentLoop(base::Bind(
            &AudioManagerMac::HandleDeviceChanges, base::Unretained(this)))));
    device_listener_first_init = true;
  }

  AudioDeviceID device = GetAudioDeviceIdByUId(false, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Failed to open output device: " << device_id;
    return NULL;
  }

  // Only set the device and sample rate if we just initialized the device
  // listener.
  if (device_listener_first_init) {
    // Only set the current output device for the default device.
    if (AudioDeviceDescription::IsDefaultDevice(device_id))
      current_output_device_ = device;
    // Just use the current sample rate since we don't allow non-native sample
    // rates on OSX.
    current_sample_rate_ = params.sample_rate();
  }

  AUHALStream* stream = new AUHALStream(this, params, device, log_callback);
  output_streams_.push_back(stream);
  return stream;
}

std::string AudioManagerMac::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
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
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  AudioInputStream* stream = new PCMQueueInAudioInputStream(this, params);
  basic_input_streams_.push_back(stream);
  return stream;
}

AudioInputStream* AudioManagerMac::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // Gets the AudioDeviceID that refers to the AudioInputDevice with the device
  // unique id. This AudioDeviceID is used to set the device for Audio Unit.
  AudioDeviceID audio_device_id = GetAudioDeviceIdByUId(true, device_id);
  AUAudioInputStream* stream = NULL;
  if (audio_device_id != kAudioObjectUnknown) {
    stream =
        new AUAudioInputStream(this, params, audio_device_id, log_callback);
    low_latency_input_streams_.push_back(stream);
  }

  return stream;
}

AudioParameters AudioManagerMac::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const AudioDeviceID device = GetAudioDeviceIdByUId(false, output_device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid output device " << output_device_id;
    return input_params.IsValid() ? input_params : AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        kFallbackSampleRate, 16, ChooseBufferSize(false, kFallbackSampleRate));
  }

  const bool has_valid_input_params = input_params.IsValid();
  const int hardware_sample_rate = HardwareSampleRateForDevice(device);

  // Allow pass through buffer sizes.  If concurrent input and output streams
  // exist, they will use the smallest buffer size amongst them.  As such, each
  // stream must be able to FIFO requests appropriately when this happens.
  int buffer_size = ChooseBufferSize(false, hardware_sample_rate);
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

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         hardware_sample_rate, 16, buffer_size);
  params.set_channels_for_discrete(output_channels);
  return params;
}

void AudioManagerMac::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  power_observer_.reset(new AudioPowerObserver());
}

void AudioManagerMac::HandleDeviceChanges() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const int new_sample_rate = HardwareSampleRate();
  AudioDeviceID new_output_device;
  GetDefaultOutputDevice(&new_output_device);

  if (current_sample_rate_ == new_sample_rate &&
      current_output_device_ == new_output_device) {
    return;
  }

  current_sample_rate_ = new_sample_rate;
  current_output_device_ = new_output_device;
  NotifyAllOutputDeviceChangeListeners();
}

int AudioManagerMac::ChooseBufferSize(bool is_input, int sample_rate) {
  // kMinimumInputOutputBufferSize is too small for the output side because
  // CoreAudio can get into under-run if the renderer fails delivering data
  // to the browser within the allowed time by the OS. The workaround is to
  // use 256 samples as the default output buffer size for sample rates
  // smaller than 96KHz.
  // TODO(xians): Remove this workaround after WebAudio supports user defined
  // buffer size.  See https://github.com/WebAudio/web-audio-api/issues/348
  // for details.
  int buffer_size = is_input ?
      kMinimumInputOutputBufferSize : 2 * kMinimumInputOutputBufferSize;
  const int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  } else if (sample_rate > 48000) {
    // The default buffer size is too small for higher sample rates and may lead
    // to glitching.  Adjust upwards by multiples of the default size.
    if (sample_rate <= 96000)
      buffer_size = 2 * kMinimumInputOutputBufferSize;
    else if (sample_rate <= 192000)
      buffer_size = 4 * kMinimumInputOutputBufferSize;
  }

  return buffer_size;
}

bool AudioManagerMac::IsSuspending() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->IsSuspending();
}

bool AudioManagerMac::ShouldDeferStreamStart() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->ShouldDeferStreamStart();
}

bool AudioManagerMac::IsOnBatteryPower() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->IsOnBatteryPower();
}

size_t AudioManagerMac::GetNumberOfResumeNotifications() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->num_resume_notifications();
}

bool AudioManagerMac::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size,
                                            bool* size_was_changed,
                                            size_t* io_buffer_frame_size) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (in_shutdown_) {
    DVLOG(1) << "Disabled since we are shutting down";
    return false;
  }
  const bool is_input = (element == 1);
  DVLOG(1) << "MaybeChangeBufferSize(id=0x" << std::hex << device_id
           << ", is_input=" << is_input << ", desired_buffer_size=" << std::dec
           << desired_buffer_size << ")";

  *size_was_changed = false;
  *io_buffer_frame_size = 0;

  // Log the device name (and id) for debugging purposes.
  std::string device_name = GetAudioDeviceNameFromDeviceId(device_id, is_input);
  DVLOG(1) << "name: " << device_name << " (ID: 0x" << std::hex << device_id
           << ")";

  // Get the current size of the I/O buffer for the specified device. The
  // property is read on a global scope, hence using element 0. The default IO
  // buffer size on Mac OSX for OS X 10.9 and later is 512 audio frames.
  UInt32 buffer_size = 0;
  UInt32 property_size = sizeof(buffer_size);
  OSStatus result = AudioUnitGetProperty(
      audio_unit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
      0, &buffer_size, &property_size);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioUnitGetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }
  // Store the currently used (not changed yet) I/O buffer frame size.
  *io_buffer_frame_size = buffer_size;

  DVLOG(1) << "current IO buffer size: " << buffer_size;
  DVLOG(1) << "#output streams: " << output_streams_.size();
  DVLOG(1) << "#input streams: " << low_latency_input_streams_.size();

  // Check if a buffer size change is required. If the caller asks for a
  // reduced size (|desired_buffer_size| < |buffer_size|), the new lower size
  // will be set. For larger buffer sizes, we have to perform some checks to
  // see if the size can actually be changed. If there is any other active
  // streams on the same device, either input or output, a larger size than
  // their requested buffer size can't be set. The reason is that an existing
  // stream can't handle buffer size larger than its requested buffer size.
  // See http://crbug.com/428706 for a reason why.

  if (buffer_size == desired_buffer_size)
    return true;

  if (desired_buffer_size > buffer_size) {
    // Do NOT set the buffer size if there is another output stream using
    // the same device with a smaller requested buffer size.
    // Note, for the caller stream, its requested_buffer_size() will be the same
    // as |desired_buffer_size|, so it won't return true due to comparing with
    // itself.
    for (auto* stream : output_streams_) {
      if (stream->device_id() == device_id &&
          stream->requested_buffer_size() < desired_buffer_size) {
        return true;
      }
    }

    // Do NOT set the buffer size if there is another input stream using
    // the same device with a smaller buffer size.
    for (auto* stream : low_latency_input_streams_) {
      if (stream->device_id() == device_id &&
          stream->requested_buffer_size() < desired_buffer_size) {
        return true;
      }
    }
  }

  // In this scope we know that the IO buffer size should be modified. But
  // first, verify that |desired_buffer_size| is within the valid range and
  // modify the desired buffer size if it is outside this range.
  // Note that, we have found that AudioUnitSetProperty(PropertyBufferFrameSize)
  // does in fact do this limitation internally and report noErr even if the
  // user tries to set an invalid size. As an example, asking for a size of
  // 4410 will on most devices be limited to 4096 without any further notice.
  UInt32 minimum, maximum;
  GetIOBufferFrameSizeRange(device_id, is_input, &minimum, &maximum);
  DVLOG(1) << "valid IO buffer size range: [" << minimum << ", " << maximum
           << "]";
  buffer_size = desired_buffer_size;
  if (buffer_size < minimum)
    buffer_size = minimum;
  else if (buffer_size > maximum)
    buffer_size = maximum;
  DVLOG(1) << "validated desired buffer size: " << buffer_size;

  // Set new (and valid) I/O buffer size for the specified device. The property
  // is set on a global scope, hence using element 0.
  result = AudioUnitSetProperty(audio_unit, kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Global, 0, &buffer_size,
                                sizeof(buffer_size));
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) failed.  "
      << "Size:: " << buffer_size;
  *size_was_changed = (result == noErr);
  DVLOG_IF(1, result == noErr) << "IO buffer size changed to: " << buffer_size;
  // Store the currently used (after a change) I/O buffer frame size.
  *io_buffer_frame_size = buffer_size;

  // If the size was changed, update the actual output buffer size used for the
  // given device ID.
  if (!is_input && (result == noErr)) {
    output_io_buffer_size_map_[device_id] = buffer_size;
  }

  return (result == noErr);
}

bool AudioManagerMac::IncreaseIOBufferSizeIfPossible(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "IncreaseIOBufferSizeIfPossible(id=0x" << std::hex << device_id
           << ")";
  if (in_shutdown_) {
    DVLOG(1) << "Disabled since we are shutting down";
    return false;
  }
  // Start by storing the actual I/O buffer size. Then scan all active output
  // streams using the specified |device_id| and find the minimum requested
  // buffer size. In addition, store a reference to the audio unit of the first
  // output stream using |device_id|.
  DCHECK(!output_io_buffer_size_map_.empty());
  // All active output streams use the same actual I/O buffer size given
  // a unique device ID.
  // TODO(henrika): it would also be possible to use AudioUnitGetProperty(...,
  // kAudioDevicePropertyBufferFrameSize,...) instead of caching the actual
  // buffer size but I have chosen to use the map instead to avoid possibly
  // expensive Core Audio API calls and the risk of failure when asking while
  // closing a stream.
  const size_t& actual_size = output_io_buffer_size_map_[device_id];
  AudioUnit audio_unit;
  size_t min_requested_size = std::numeric_limits<std::size_t>::max();
  for (auto* stream : output_streams_) {
    if (stream->device_id() == device_id) {
      if (min_requested_size == std::numeric_limits<std::size_t>::max()) {
        // Store reference to the first audio unit using the specified ID.
        audio_unit = stream->audio_unit();
      }
      if (stream->requested_buffer_size() < min_requested_size)
        min_requested_size = stream->requested_buffer_size();
      DVLOG(1) << "requested:" << stream->requested_buffer_size()
               << " actual: " << actual_size;
    }
  }

  if (min_requested_size == std::numeric_limits<std::size_t>::max()) {
    DVLOG(1) << "No action since there is no active stream for given device id";
    return false;
  }

  // It is only possible to revert to a larger buffer size if the lowest
  // requested is not in use. Example: if the actual I/O buffer size is 256 and
  // at least one output stream has asked for 256 as its buffer size, we can't
  // start using a larger I/O buffer size.
  DCHECK_GE(min_requested_size, actual_size);
  if (min_requested_size == actual_size) {
    DVLOG(1) << "No action since lowest possible size is already in use: "
             << actual_size;
    return false;
  }

  // It should now be safe to increase the I/O buffer size to a new (higher)
  // value using the |min_requested_size|. Doing so will save system resources.
  // All active output streams with the same |device_id| are affected by this
  // change but it is only required to apply the change to one of the streams.
  DVLOG(1) << "min_requested_size: " << min_requested_size;
  bool size_was_changed = false;
  size_t io_buffer_frame_size = 0;
  bool result =
      MaybeChangeBufferSize(device_id, audio_unit, 0, min_requested_size,
                            &size_was_changed, &io_buffer_frame_size);
  DCHECK_EQ(io_buffer_frame_size, min_requested_size);
  DCHECK(size_was_changed);
  return result;
}

bool AudioManagerMac::AudioDeviceIsUsedForInput(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!basic_input_streams_.empty()) {
    // For Audio Queues and in the default case (Mac OS X), the audio comes
    // from the system’s default audio input device as set by a user in System
    // Preferences.
    AudioDeviceID default_id;
    GetDefaultDevice(&default_id, true);
    if (default_id == device_id)
      return true;
  }

  // Each low latency streams has its own device ID.
  for (auto* stream : low_latency_input_streams_) {
    if (stream->device_id() == device_id)
      return true;
  }
  return false;
}

void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerMac::ReleaseOutputStreamUsingRealDevice(
    AudioOutputStream* stream,
    AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "Closing output stream with id=0x" << std::hex << device_id;
  DVLOG(1) << "requested_buffer_size: "
           << static_cast<AUHALStream*>(stream)->requested_buffer_size();

  // Start by closing down the specified output stream.
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);

  // Prevent attempt to alter buffer size if the released stream was the last
  // output stream.
  if (output_streams_.empty())
    return;

  if (!AudioDeviceIsUsedForInput(device_id)) {
    // The current audio device is not used for input. See if it is possible to
    // increase the IO buffer size (saves power) given the remaining output
    // audio streams and their buffer size requirements.
    IncreaseIOBufferSizeIfPossible(device_id);
  }
}

void AudioManagerMac::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  auto stream_it = std::find(basic_input_streams_.begin(),
                             basic_input_streams_.end(),
                             stream);
  if (stream_it == basic_input_streams_.end())
    low_latency_input_streams_.remove(static_cast<AUAudioInputStream*>(stream));
  else
    basic_input_streams_.erase(stream_it);

  AudioManagerBase::ReleaseInputStream(stream);
}

ScopedAudioManagerPtr CreateAudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory) {
  return ScopedAudioManagerPtr(
      new AudioManagerMac(std::move(task_runner), std::move(worker_task_runner),
                          audio_log_factory));
}

}  // namespace media
