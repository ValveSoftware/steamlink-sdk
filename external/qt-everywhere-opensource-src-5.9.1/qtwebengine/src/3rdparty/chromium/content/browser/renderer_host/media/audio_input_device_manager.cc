// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

namespace content {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;

namespace {
// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;
}

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioManager* audio_manager)
    : listener_(NULL),
      next_capture_session_id_(kFirstSessionId),
#if defined(OS_CHROMEOS)
      keyboard_mic_streams_count_(0),
#endif
      audio_manager_(audio_manager) {
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

const StreamDeviceInfo* AudioInputDeviceManager::GetOpenedDeviceInfoById(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return NULL;

  return &(*device);
}

void AudioInputDeviceManager::Register(
    MediaStreamProviderListener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listener_);
  DCHECK(!device_task_runner_.get());
  listener_ = listener;
  device_task_runner_ = device_task_runner;
}

void AudioInputDeviceManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

int AudioInputDeviceManager::Open(const StreamDeviceInfo& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Generate a new id for this device.
  int session_id = next_capture_session_id_++;
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::OpenOnDeviceThread,
                 this, session_id, device));

  return session_id;
}

void AudioInputDeviceManager::Close(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return;
  const MediaStreamType stream_type = device->device.type;
  if (session_id != kFakeOpenSessionId)
    devices_.erase(device);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager is expecting the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::ClosedOnIOThread,
                                     this, stream_type, session_id));
}

#if defined(OS_CHROMEOS)
void AudioInputDeviceManager::RegisterKeyboardMicStream(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++keyboard_mic_streams_count_;
  if (keyboard_mic_streams_count_ == 1) {
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            &AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread,
            this,
            true),
        callback);
  } else {
    callback.Run();
  }
}

void AudioInputDeviceManager::UnregisterKeyboardMicStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  --keyboard_mic_streams_count_;
  DCHECK_GE(keyboard_mic_streams_count_, 0);
  if (keyboard_mic_streams_count_ == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            &AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread,
            this,
            false));
  }
}
#endif

void AudioInputDeviceManager::OpenOnDeviceThread(
    int session_id, const StreamDeviceInfo& info) {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.AudioInputDeviceManager.OpenOnDeviceThreadTime");
  DCHECK(IsOnDeviceThread());

  StreamDeviceInfo out(info.device.type, info.device.name, info.device.id, 0, 0,
                       0);
  out.session_id = session_id;

  MediaStreamDevice::AudioDeviceParameters& input_params = out.device.input;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    // Don't need to query the hardware information if using fake device.
    input_params.sample_rate = 44100;
    input_params.channel_layout = media::CHANNEL_LAYOUT_STEREO;
  } else {
    // Get the preferred sample rate and channel configuration for the
    // audio device.
    media::AudioParameters params =
        audio_manager_->GetInputStreamParameters(info.device.id);
    input_params.sample_rate = params.sample_rate();
    input_params.channel_layout = params.channel_layout();
    input_params.frames_per_buffer = params.frames_per_buffer();
    input_params.effects = params.effects();
    input_params.mic_positions = params.mic_positions();

    // Add preferred output device information if a matching output device
    // exists.
    out.device.matched_output_device_id =
        audio_manager_->GetAssociatedOutputDeviceID(info.device.id);
    if (!out.device.matched_output_device_id.empty()) {
      params = audio_manager_->GetOutputStreamParameters(
          out.device.matched_output_device_id);
      MediaStreamDevice::AudioDeviceParameters& matched_output_params =
          out.device.matched_output;
      matched_output_params.sample_rate = params.sample_rate();
      matched_output_params.channel_layout = params.channel_layout();
      matched_output_params.frames_per_buffer = params.frames_per_buffer();
    }
  }

  // Return the |session_id| through the listener by posting a task on
  // IO thread since MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::OpenedOnIOThread,
                                     this, session_id, out));
}

void AudioInputDeviceManager::OpenedOnIOThread(int session_id,
                                               const StreamDeviceInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(session_id, info.session_id);
  DCHECK(GetDevice(session_id) == devices_.end());

  devices_.push_back(info);

  if (listener_)
    listener_->Opened(info.device.type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(MediaStreamType stream_type,
                                               int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (listener_)
    listener_->Closed(stream_type, session_id);
}

bool AudioInputDeviceManager::IsOnDeviceThread() const {
  return device_task_runner_->BelongsToCurrentThread();
}

AudioInputDeviceManager::StreamDeviceList::iterator
AudioInputDeviceManager::GetDevice(int session_id) {
  for (StreamDeviceList::iterator i(devices_.begin()); i != devices_.end();
       ++i) {
    if (i->session_id == session_id)
      return i;
  }

  return devices_.end();
}

#if defined(OS_CHROMEOS)
void AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread(
    bool active) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  chromeos::CrasAudioHandler::Get()->SetKeyboardMicActive(active);
}
#endif


}  // namespace content
