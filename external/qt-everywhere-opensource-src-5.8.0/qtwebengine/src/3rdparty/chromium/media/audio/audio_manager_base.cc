// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/base/media_switches.h"

namespace media {
namespace {

const int kStreamCloseDelaySeconds = 5;

// Default maximum number of output streams that can be open simultaneously
// for all platforms.
const int kDefaultMaxOutputStreams = 16;

// Default maximum number of input streams that can be open simultaneously
// for all platforms.
const int kDefaultMaxInputStreams = 16;

const int kMaxInputChannels = 3;

}  // namespace

struct AudioManagerBase::DispatcherParams {
  DispatcherParams(const AudioParameters& input,
                   const AudioParameters& output,
                   const std::string& output_device_id)
      : input_params(input),
        output_params(output),
        output_device_id(output_device_id) {}
  ~DispatcherParams() {}

  const AudioParameters input_params;
  const AudioParameters output_params;
  const std::string output_device_id;
  scoped_refptr<AudioOutputDispatcher> dispatcher;

 private:
  DISALLOW_COPY_AND_ASSIGN(DispatcherParams);
};

class AudioManagerBase::CompareByParams {
 public:
  explicit CompareByParams(const DispatcherParams* dispatcher)
      : dispatcher_(dispatcher) {}
  bool operator()(DispatcherParams* dispatcher_in) const {
    // We will reuse the existing dispatcher when:
    // 1) Unified IO is not used, input_params and output_params of the
    //    existing dispatcher are the same as the requested dispatcher.
    // 2) Unified IO is used, input_params and output_params of the existing
    //    dispatcher are the same as the request dispatcher.
    return (dispatcher_->input_params.Equals(dispatcher_in->input_params) &&
            dispatcher_->output_params.Equals(dispatcher_in->output_params) &&
            dispatcher_->output_device_id == dispatcher_in->output_device_id);
  }

 private:
  const DispatcherParams* dispatcher_;
};

AudioManagerBase::AudioManagerBase(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory)
    : AudioManager(std::move(task_runner), std::move(worker_task_runner)),
      max_num_output_streams_(kDefaultMaxOutputStreams),
      max_num_input_streams_(kDefaultMaxInputStreams),
      num_output_streams_(0),
      num_input_streams_(0),
      // TODO(dalecurtis): Switch this to an base::ObserverListThreadSafe, so we
      // don't
      // block the UI thread when swapping devices.
      output_listeners_(
          base::ObserverList<AudioDeviceListener>::NOTIFY_EXISTING_ONLY),
      audio_log_factory_(audio_log_factory) {}

AudioManagerBase::~AudioManagerBase() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // All the output streams should have been deleted.
  CHECK_EQ(0, num_output_streams_);
  // All the input streams should have been deleted.
  CHECK_EQ(0, num_input_streams_);
}

base::string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return base::string16();
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (!params.IsValid()) {
    DLOG(ERROR) << "Audio parameters are invalid";
    return NULL;
  }

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242.
  if (num_output_streams_ >= max_num_output_streams_) {
    DLOG(ERROR) << "Number of opened output audio streams "
                << num_output_streams_
                << " exceed the max allowed number "
                << max_num_output_streams_;
    return NULL;
  }

  AudioOutputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      DCHECK(AudioDeviceDescription::IsDefaultDevice(device_id))
          << "AUDIO_PCM_LINEAR supports only the default device.";
      stream = MakeLinearOutputStream(params, log_callback);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyOutputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioOutputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream) {
    ++num_output_streams_;
  }

  return stream;
}

AudioInputStream* AudioManagerBase::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (!params.IsValid() || (params.channels() > kMaxInputChannels) ||
      device_id.empty()) {
    DLOG(ERROR) << "Audio parameters are invalid for device " << device_id;
    return NULL;
  }

  if (num_input_streams_ >= max_num_input_streams_) {
    DLOG(ERROR) << "Number of opened input audio streams "
                << num_input_streams_
                << " exceed the max allowed number " << max_num_input_streams_;
    return NULL;
  }

  DVLOG(2) << "Creating a new AudioInputStream with buffer size = "
           << params.frames_per_buffer();

  AudioInputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      stream = MakeLinearInputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyInputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioInputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream) {
    ++num_input_streams_;
  }

  return stream;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params,
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // If the caller supplied an empty device id to select the default device,
  // we fetch the actual device id of the default device so that the lookup
  // will find the correct device regardless of whether it was opened as
  // "default" or via the specific id.
  // NOTE: Implementations that don't yet support opening non-default output
  // devices may return an empty string from GetDefaultOutputDeviceID().
  std::string output_device_id =
      AudioDeviceDescription::IsDefaultDevice(device_id)
          ? GetDefaultOutputDeviceID()
          : device_id;

  // If we're not using AudioOutputResampler our output parameters are the same
  // as our input parameters.
  AudioParameters output_params = params;
  if (params.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    output_params =
        GetPreferredOutputStreamParameters(output_device_id, params);

    // Ensure we only pass on valid output parameters.
    if (!output_params.IsValid()) {
      // We've received invalid audio output parameters, so switch to a mock
      // output device based on the input parameters.  This may happen if the OS
      // provided us junk values for the hardware configuration.
      LOG(ERROR) << "Invalid audio output parameters received; using fake "
                 << "audio path. Channels: " << output_params.channels() << ", "
                 << "Sample Rate: " << output_params.sample_rate() << ", "
                 << "Bits Per Sample: " << output_params.bits_per_sample()
                 << ", Frames Per Buffer: "
                 << output_params.frames_per_buffer();

      // Tell the AudioManager to create a fake output device.
      output_params = params;
      output_params.set_format(AudioParameters::AUDIO_FAKE);
    } else if (params.effects() != output_params.effects()) {
      // Turn off effects that weren't requested.
      output_params.set_effects(params.effects() & output_params.effects());
    }
  }

  DispatcherParams* dispatcher_params =
      new DispatcherParams(params, output_params, output_device_id);

  AudioOutputDispatchers::iterator it =
      std::find_if(output_dispatchers_.begin(), output_dispatchers_.end(),
                   CompareByParams(dispatcher_params));
  if (it != output_dispatchers_.end()) {
    delete dispatcher_params;
    return new AudioOutputProxy((*it)->dispatcher.get());
  }

  const base::TimeDelta kCloseDelay =
      base::TimeDelta::FromSeconds(kStreamCloseDelaySeconds);
  scoped_refptr<AudioOutputDispatcher> dispatcher;
  if (output_params.format() != AudioParameters::AUDIO_FAKE) {
    dispatcher = new AudioOutputResampler(this, params, output_params,
                                          output_device_id,
                                          kCloseDelay);
  } else {
    dispatcher = new AudioOutputDispatcherImpl(this, output_params,
                                               output_device_id,
                                               kCloseDelay);
  }

  dispatcher_params->dispatcher = dispatcher;
  output_dispatchers_.push_back(dispatcher_params);
  return new AudioOutputProxy(dispatcher.get());
}

void AudioManagerBase::ShowAudioInputSettings() {
}

void AudioManagerBase::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioOutputStream.
  // For example, pass the ownership to AudioManager so it can delete the
  // streams.
  --num_output_streams_;
  delete stream;
}

void AudioManagerBase::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioInputStream.
  --num_input_streams_;
  delete stream;
}

void AudioManagerBase::Shutdown() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  // Close all output streams.
  while (!output_dispatchers_.empty()) {
    output_dispatchers_.back()->dispatcher->Shutdown();
    output_dispatchers_.pop_back();
  }
}

void AudioManagerBase::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_listeners_.AddObserver(listener);
}

void AudioManagerBase::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_listeners_.RemoveObserver(listener);
}

void AudioManagerBase::NotifyAllOutputDeviceChangeListeners() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "Firing OnDeviceChange() notifications.";
  FOR_EACH_OBSERVER(AudioDeviceListener, output_listeners_, OnDeviceChange());
}

AudioParameters AudioManagerBase::GetDefaultOutputStreamParameters() {
  return GetPreferredOutputStreamParameters(GetDefaultOutputDeviceID(),
      AudioParameters());
}

AudioParameters AudioManagerBase::GetOutputStreamParameters(
    const std::string& device_id) {
  return GetPreferredOutputStreamParameters(device_id,
      AudioParameters());
}

AudioParameters AudioManagerBase::GetInputStreamParameters(
    const std::string& device_id) {
  NOTREACHED();
  return AudioParameters();
}

std::string AudioManagerBase::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return "";
}

std::string AudioManagerBase::GetDefaultOutputDeviceID() {
  return "";
}

int AudioManagerBase::GetUserBufferSize() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  int buffer_size = 0;
  std::string buffer_size_str(cmd_line->GetSwitchValueASCII(
      switches::kAudioBufferSize));
  if (base::StringToInt(buffer_size_str, &buffer_size) && buffer_size > 0)
    return buffer_size;

  return 0;
}

std::unique_ptr<AudioLog> AudioManagerBase::CreateAudioLog(
    AudioLogFactory::AudioComponent component) {
  return audio_log_factory_->CreateAudioLog(component);
}

}  // namespace media
