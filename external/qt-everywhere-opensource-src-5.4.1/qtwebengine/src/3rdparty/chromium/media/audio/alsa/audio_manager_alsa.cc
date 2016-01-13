// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alsa/audio_manager_alsa.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/nix/xdg_util.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"
#if defined(USE_CRAS)
#include "media/audio/cras/audio_manager_cras.h"
#endif
#include "media/audio/alsa/alsa_input.h"
#include "media/audio/alsa/alsa_output.h"
#include "media/audio/alsa/alsa_wrapper.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
static const int kDefaultSampleRate = 48000;

// Since "default", "pulse" and "dmix" devices are virtual devices mapped to
// real devices, we remove them from the list to avoiding duplicate counting.
// In addition, note that we support no more than 2 channels for recording,
// hence surround devices are not stored in the list.
static const char* kInvalidAudioInputDevices[] = {
  "default",
  "dmix",
  "null",
  "pulse",
  "surround",
};

// static
void AudioManagerAlsa::ShowLinuxAudioInputSettings() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  CommandLine command_line(CommandLine::NO_PROGRAM);
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      command_line.SetProgram(base::FilePath("gnome-volume-control"));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      command_line.SetProgram(base::FilePath("kmix"));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
      command_line.SetProgram(base::FilePath("gnome-control-center"));
      command_line.AppendArg("sound");
      command_line.AppendArg("input");
      break;
    default:
      LOG(ERROR) << "Failed to show audio input settings: we don't know "
                 << "what command to use for your desktop environment.";
      return;
  }
  base::LaunchProcess(command_line, base::LaunchOptions(), NULL);
}

// Implementation of AudioManager.
bool AudioManagerAlsa::HasAudioOutputDevices() {
  return HasAnyAlsaAudioDevice(kStreamPlayback);
}

bool AudioManagerAlsa::HasAudioInputDevices() {
  return HasAnyAlsaAudioDevice(kStreamCapture);
}

AudioManagerAlsa::AudioManagerAlsa(AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory),
      wrapper_(new AlsaWrapper()) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAlsa::~AudioManagerAlsa() {
  Shutdown();
}

void AudioManagerAlsa::ShowAudioInputSettings() {
  ShowLinuxAudioInputSettings();
}

void AudioManagerAlsa::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAlsaAudioDevices(kStreamCapture, device_names);
}

void AudioManagerAlsa::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAlsaAudioDevices(kStreamPlayback, device_names);
}

AudioParameters AudioManagerAlsa::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, 16, kDefaultInputBufferSize);
}

void AudioManagerAlsa::GetAlsaAudioDevices(
    StreamType type,
    media::AudioDeviceNames* device_names) {
  // Constants specified by the ALSA API for device hints.
  static const char kPcmInterfaceName[] = "pcm";
  int card = -1;

  // Loop through the sound cards to get ALSA device hints.
  while (!wrapper_->CardNext(&card) && card >= 0) {
    void** hints = NULL;
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      GetAlsaDevicesInfo(type, hints, device_names);

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
    } else {
      DLOG(WARNING) << "GetAlsaAudioDevices: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }
}

void AudioManagerAlsa::GetAlsaDevicesInfo(
    AudioManagerAlsa::StreamType type,
    void** hints,
    media::AudioDeviceNames* device_names) {
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";
  static const char kDescriptionHintName[] = "DESC";

  const char* unwanted_device_type = UnwantedDeviceTypeWhenEnumerating(type);

  for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
    // Only examine devices of the right type.  Valid values are
    // "Input", "Output", and NULL which means both input and output.
    scoped_ptr<char, base::FreeDeleter> io(wrapper_->DeviceNameGetHint(
        *hint_iter, kIoHintName));
    if (io != NULL && strcmp(unwanted_device_type, io.get()) == 0)
      continue;

    // Found a device, prepend the default device since we always want
    // it to be on the top of the list for all platforms. And there is
    // no duplicate counting here since it is only done if the list is
    // still empty.  Note, pulse has exclusively opened the default
    // device, so we must open the device via the "default" moniker.
    if (device_names->empty()) {
      device_names->push_front(media::AudioDeviceName(
          AudioManagerBase::kDefaultDeviceName,
          AudioManagerBase::kDefaultDeviceId));
    }

    // Get the unique device name for the device.
    scoped_ptr<char, base::FreeDeleter> unique_device_name(
        wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));

    // Find out if the device is available.
    if (IsAlsaDeviceAvailable(type, unique_device_name.get())) {
      // Get the description for the device.
      scoped_ptr<char, base::FreeDeleter> desc(wrapper_->DeviceNameGetHint(
          *hint_iter, kDescriptionHintName));

      media::AudioDeviceName name;
      name.unique_id = unique_device_name.get();
      if (desc) {
        // Use the more user friendly description as name.
        // Replace '\n' with '-'.
        char* pret = strchr(desc.get(), '\n');
        if (pret)
          *pret = '-';
        name.device_name = desc.get();
      } else {
        // Virtual devices don't necessarily have descriptions.
        // Use their names instead.
        name.device_name = unique_device_name.get();
      }

      // Store the device information.
      device_names->push_back(name);
    }
  }
}

// static
bool AudioManagerAlsa::IsAlsaDeviceAvailable(
    AudioManagerAlsa::StreamType type,
    const char* device_name) {
  if (!device_name)
    return false;

  // We do prefix matches on the device name to see whether to include
  // it or not.
  if (type == kStreamCapture) {
    // Check if the device is in the list of invalid devices.
    for (size_t i = 0; i < arraysize(kInvalidAudioInputDevices); ++i) {
      if (strncmp(kInvalidAudioInputDevices[i], device_name,
                  strlen(kInvalidAudioInputDevices[i])) == 0)
        return false;
    }
    return true;
  } else {
    DCHECK_EQ(kStreamPlayback, type);
    // We prefer the device type that maps straight to hardware but
    // goes through software conversion if needed (e.g. incompatible
    // sample rate).
    // TODO(joi): Should we prefer "hw" instead?
    static const char kDeviceTypeDesired[] = "plughw";
    return strncmp(kDeviceTypeDesired,
                   device_name,
                   arraysize(kDeviceTypeDesired) - 1) == 0;
  }
}

// static
const char* AudioManagerAlsa::UnwantedDeviceTypeWhenEnumerating(
    AudioManagerAlsa::StreamType wanted_type) {
  return wanted_type == kStreamPlayback ? "Input" : "Output";
}

bool AudioManagerAlsa::HasAnyAlsaAudioDevice(
    AudioManagerAlsa::StreamType stream) {
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  void** hints = NULL;
  bool has_device = false;
  int card = -1;

  // Loop through the sound cards.
  // Don't use snd_device_name_hint(-1,..) since there is a access violation
  // inside this ALSA API with libasound.so.2.0.0.
  while (!wrapper_->CardNext(&card) && (card >= 0) && !has_device) {
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
        // Only examine devices that are |stream| capable.  Valid values are
        // "Input", "Output", and NULL which means both input and output.
        scoped_ptr<char, base::FreeDeleter> io(wrapper_->DeviceNameGetHint(
            *hint_iter, kIoHintName));
        const char* unwanted_type = UnwantedDeviceTypeWhenEnumerating(stream);
        if (io != NULL && strcmp(unwanted_type, io.get()) == 0)
          continue;  // Wrong type, skip the device.

        // Found an input device.
        has_device = true;
        break;
      }

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
      hints = NULL;
    } else {
      DLOG(WARNING) << "HasAnyAudioDevice: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }

  return has_device;
}

AudioOutputStream* AudioManagerAlsa::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerAlsa::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerAlsa::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerAlsa::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerAlsa::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  static const int kDefaultOutputBufferSize = 2048;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    // Some clients, such as WebRTC, have a more limited use case and work
    // acceptably with a smaller buffer size.  The check below allows clients
    // which want to try a smaller buffer size on Linux to do so.
    // TODO(dalecurtis): This should include bits per channel and channel layout
    // eventually.
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = std::min(input_params.frames_per_buffer(), buffer_size);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size, AudioParameters::NO_EFFECTS);
}

AudioOutputStream* AudioManagerAlsa::MakeOutputStream(
    const AudioParameters& params) {
  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlsaOutputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaOutputDevice);
  }
  return new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this);
}

AudioInputStream* AudioManagerAlsa::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  std::string device_name = (device_id == AudioManagerBase::kDefaultDeviceId) ?
      AlsaPcmInputStream::kAutoSelectDevice : device_id;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAlsaInputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaInputDevice);
  }

  return new AlsaPcmInputStream(this, device_name, params, wrapper_.get());
}

}  // namespace media
