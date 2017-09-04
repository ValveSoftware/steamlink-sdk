// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <stddef.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/media_resources.h"

// cras_util.h headers pull in min/max macros...
// TODO(dgreid): Fix headers such that these aren't imported.
#undef min
#undef max

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
const int kDefaultSampleRate = 48000;

// Define bounds for the output buffer size.
const int kMinimumOutputBufferSize = 512;
const int kMaximumOutputBufferSize = 8192;

// Default input buffer size.
const int kDefaultInputBufferSize = 1024;

const char kBeamformingOnDeviceId[] = "default-beamforming-on";
const char kBeamformingOffDeviceId[] = "default-beamforming-off";

enum CrosBeamformingDeviceState {
  BEAMFORMING_DEFAULT_ENABLED = 0,
  BEAMFORMING_USER_ENABLED,
  BEAMFORMING_DEFAULT_DISABLED,
  BEAMFORMING_USER_DISABLED,
  BEAMFORMING_STATE_MAX = BEAMFORMING_USER_DISABLED
};

void RecordBeamformingDeviceState(CrosBeamformingDeviceState state) {
  UMA_HISTOGRAM_ENUMERATION("Media.CrosBeamformingDeviceState", state,
                            BEAMFORMING_STATE_MAX + 1);
}

bool IsBeamformingDefaultEnabled() {
  return base::FieldTrialList::FindFullName("ChromebookBeamforming") ==
         "Enabled";
}

// Returns a mic positions string if the machine has a beamforming capable
// internal mic and otherwise an empty string.
std::string MicPositions() {
  // Get the list of devices from CRAS. An internal mic with a non-empty
  // positions field indicates the machine has a beamforming capable mic array.
  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (const auto& device : devices) {
    if (device.type == chromeos::AUDIO_TYPE_INTERNAL_MIC) {
      // There should be only one internal mic device.
      return device.mic_positions;
    }
  }
  return "";
}

}  // namespace

// Adds the beamforming on and off devices to |device_names|.
void AudioManagerCras::AddBeamformingDevices(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  const std::string beamforming_on_name =
      GetLocalizedStringUTF8(BEAMFORMING_ON_DEFAULT_AUDIO_INPUT_DEVICE_NAME);
  const std::string beamforming_off_name =
      GetLocalizedStringUTF8(BEAMFORMING_OFF_DEFAULT_AUDIO_INPUT_DEVICE_NAME);

  if (IsBeamformingDefaultEnabled()) {
    // The first device in the list is expected to have a "default" device ID.
    // Web apps may depend on this behavior.
    beamforming_on_device_id_ = AudioDeviceDescription::kDefaultDeviceId;
    beamforming_off_device_id_ = kBeamformingOffDeviceId;

    // Users in the experiment will have the "beamforming on" device appear
    // first in the list. This causes it to be selected by default.
    device_names->push_back(
        AudioDeviceName(beamforming_on_name, beamforming_on_device_id_));
    device_names->push_back(
        AudioDeviceName(beamforming_off_name, beamforming_off_device_id_));
  } else {
    beamforming_off_device_id_ = AudioDeviceDescription::kDefaultDeviceId;
    beamforming_on_device_id_ = kBeamformingOnDeviceId;

    device_names->push_back(
        AudioDeviceName(beamforming_off_name, beamforming_off_device_id_));
    device_names->push_back(
        AudioDeviceName(beamforming_on_name, beamforming_on_device_id_));
  }
}

bool AudioManagerCras::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerCras::HasAudioInputDevices() {
  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_input && devices[i].is_for_simple_usage())
      return true;
  }
  return false;
}

AudioManagerCras::AudioManagerCras(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(task_runner),
                       std::move(worker_task_runner),
                       audio_log_factory),
      beamforming_on_device_id_(nullptr),
      beamforming_off_device_id_(nullptr) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCras::~AudioManagerCras() {
  Shutdown();
}

void AudioManagerCras::ShowAudioInputSettings() {
  NOTIMPLEMENTED();
}

void AudioManagerCras::GetAudioDeviceNamesImpl(bool is_input,
                                               AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  // At least two mic positions indicates we have a beamforming capable mic
  // array. Add the virtual beamforming device to the list. When this device is
  // queried through GetInputStreamParameters, provide the cached mic positions.
  if (is_input && mic_positions_.size() > 1)
    AddBeamformingDevices(device_names);
  else
    device_names->push_back(media::AudioDeviceName::CreateDefault());

  if (base::FeatureList::IsEnabled(features::kEnumerateAudioDevices)) {
    chromeos::AudioDeviceList devices;
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
    for (const auto& device : devices) {
      if (device.is_input == is_input && device.is_for_simple_usage()) {
        device_names->emplace_back(device.display_name,
                                   base::Uint64ToString(device.id));
      }
    }
  }
}

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  mic_positions_ = ParsePointsFromString(MicPositions());
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  int buffer_size = user_buffer_size ?
      user_buffer_size : kDefaultInputBufferSize;

  // TODO(hshi): Fine-tune audio parameters based on |device_id|. The optimal
  // parameters for the loopback stream may differ from the default.
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, kDefaultSampleRate, 16,
                         buffer_size);
  if (chromeos::CrasAudioHandler::Get()->HasKeyboardMic())
    params.set_effects(AudioParameters::KEYBOARD_MIC);

  if (mic_positions_.size() > 1) {
    // We have the mic_positions_ check here because one of the beamforming
    // devices will have been assigned the "default" ID, which could otherwise
    // be confused with the ID in the non-beamforming-capable-device case.
    DCHECK(beamforming_on_device_id_);
    DCHECK(beamforming_off_device_id_);

    if (device_id == beamforming_on_device_id_) {
      params.set_mic_positions(mic_positions_);

      // Record a UMA metric based on the state of the experiment and the
      // selected device. This will tell us i) how common it is for users to
      // manually adjust the beamforming device and ii) how contaminated our
      // metric experiment buckets are.
      if (IsBeamformingDefaultEnabled())
        RecordBeamformingDeviceState(BEAMFORMING_DEFAULT_ENABLED);
      else
        RecordBeamformingDeviceState(BEAMFORMING_USER_ENABLED);
    } else if (device_id == beamforming_off_device_id_) {
      if (!IsBeamformingDefaultEnabled())
        RecordBeamformingDeviceState(BEAMFORMING_DEFAULT_DISABLED);
      else
        RecordBeamformingDeviceState(BEAMFORMING_USER_DISABLED);
    }
  }
  return params;
}

AudioOutputStream* AudioManagerCras::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerCras::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // TODO(dgreid): Open the correct input device for unified IO.
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerCras::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kMinimumOutputBufferSize;
  int bits_per_sample = 16;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    buffer_size =
        std::min(kMaximumOutputBufferSize,
                 std::max(buffer_size, input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, bits_per_sample, buffer_size);
}

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params) {
  return new CrasUnifiedStream(params, this);
}

AudioInputStream* AudioManagerCras::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this, device_id);
}

snd_pcm_format_t AudioManagerCras::BitsToFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return SND_PCM_FORMAT_U8;
    case 16:
      return SND_PCM_FORMAT_S16;
    case 24:
      return SND_PCM_FORMAT_S24;
    case 32:
      return SND_PCM_FORMAT_S32;
    default:
      return SND_PCM_FORMAT_UNKNOWN;
  }
}

}  // namespace media
