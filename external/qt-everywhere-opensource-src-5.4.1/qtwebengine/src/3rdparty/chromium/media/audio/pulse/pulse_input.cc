// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_input.h"

#include <pulse/pulseaudio.h>

#include "base/logging.h"
#include "media/audio/pulse/audio_manager_pulse.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/seekable_buffer.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

PulseAudioInputStream::PulseAudioInputStream(AudioManagerPulse* audio_manager,
                                             const std::string& device_name,
                                             const AudioParameters& params,
                                             pa_threaded_mainloop* mainloop,
                                             pa_context* context)
    : audio_manager_(audio_manager),
      callback_(NULL),
      device_name_(device_name),
      params_(params),
      channels_(0),
      volume_(0.0),
      stream_started_(false),
      pa_mainloop_(mainloop),
      pa_context_(context),
      handle_(NULL),
      context_state_changed_(false) {
  DCHECK(mainloop);
  DCHECK(context);
  CHECK(params_.IsValid());
  audio_bus_ = AudioBus::Create(params_);
}

PulseAudioInputStream::~PulseAudioInputStream() {
  // All internal structures should already have been freed in Close(),
  // which calls AudioManagerPulse::Release which deletes this object.
  DCHECK(!handle_);
}

bool PulseAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!pulse::CreateInputStream(pa_mainloop_, pa_context_, &handle_, params_,
                                device_name_, &StreamNotifyCallback, this)) {
    return false;
  }

  DCHECK(handle_);

  buffer_.reset(new media::SeekableBuffer(0, 2 * params_.GetBytesPerBuffer()));
  audio_data_buffer_.reset(new uint8[params_.GetBytesPerBuffer()]);
  return true;
}

void PulseAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(handle_);

  // AGC needs to be started out of the lock.
  StartAgc();

  AutoPulseLock auto_lock(pa_mainloop_);

  if (stream_started_)
    return;

  // Clean up the old buffer.
  pa_stream_drop(handle_);
  buffer_->Clear();

  // Start the streaming.
  callback_ = callback;
  pa_stream_set_read_callback(handle_, &ReadCallback, this);
  pa_stream_readable_size(handle_);
  stream_started_ = true;

  pa_operation* operation = pa_stream_cork(handle_, 0, NULL, NULL);
  WaitForOperationCompletion(pa_mainloop_, operation);
}

void PulseAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!stream_started_)
    return;

  StopAgc();

  // Set the flag to false to stop filling new data to soundcard.
  stream_started_ = false;

  pa_operation* operation = pa_stream_flush(handle_,
                                            &pulse::StreamSuccessCallback,
                                            pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);

  // Stop the stream.
  pa_stream_set_read_callback(handle_, NULL, NULL);
  operation = pa_stream_cork(handle_, 1, &pulse::StreamSuccessCallback,
                             pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);
  callback_ = NULL;
}

void PulseAudioInputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    AutoPulseLock auto_lock(pa_mainloop_);
    if (handle_) {
      // Disable all the callbacks before disconnecting.
      pa_stream_set_state_callback(handle_, NULL, NULL);
      pa_stream_flush(handle_, NULL, NULL);

      if (pa_stream_get_state(handle_) != PA_STREAM_UNCONNECTED)
        pa_stream_disconnect(handle_);

      // Release PulseAudio structures.
      pa_stream_unref(handle_);
      handle_ = NULL;
    }
  }

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  audio_manager_->ReleaseInputStream(this);
}

double PulseAudioInputStream::GetMaxVolume() {
  return static_cast<double>(PA_VOLUME_NORM);
}

void PulseAudioInputStream::SetVolume(double volume) {
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!handle_)
    return;

  size_t index = pa_stream_get_device_index(handle_);
  pa_operation* operation = NULL;
  if (!channels_) {
    // Get the number of channels for the source only when the |channels_| is 0.
    // We are assuming the stream source is not changed on the fly here.
    operation = pa_context_get_source_info_by_index(
        pa_context_, index, &VolumeCallback, this);
    WaitForOperationCompletion(pa_mainloop_, operation);
    if (!channels_) {
      DLOG(WARNING) << "Failed to get the number of channels for the source";
      return;
    }
  }

  pa_cvolume pa_volume;
  pa_cvolume_set(&pa_volume, channels_, volume);
  operation = pa_context_set_source_volume_by_index(
      pa_context_, index, &pa_volume, NULL, NULL);

  // Don't need to wait for this task to complete.
  pa_operation_unref(operation);
}

double PulseAudioInputStream::GetVolume() {
  if (pa_threaded_mainloop_in_thread(pa_mainloop_)) {
    // When being called by the pulse thread, GetVolume() is asynchronous and
    // called under AutoPulseLock.
    if (!handle_)
      return 0.0;

    size_t index = pa_stream_get_device_index(handle_);
    pa_operation* operation = pa_context_get_source_info_by_index(
        pa_context_, index, &VolumeCallback, this);
    // Do not wait for the operation since we can't block the pulse thread.
    pa_operation_unref(operation);

    // Return zero and the callback will asynchronously update the |volume_|.
    return 0.0;
  } else {
    // Called by other thread, put an AutoPulseLock and wait for the operation.
    AutoPulseLock auto_lock(pa_mainloop_);
    if (!handle_)
      return 0.0;

    size_t index = pa_stream_get_device_index(handle_);
    pa_operation* operation = pa_context_get_source_info_by_index(
        pa_context_, index, &VolumeCallback, this);
    WaitForOperationCompletion(pa_mainloop_, operation);

    return volume_;
  }
}

// static, used by pa_stream_set_read_callback.
void PulseAudioInputStream::ReadCallback(pa_stream* handle,
                                         size_t length,
                                         void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  stream->ReadData();
}

// static, used by pa_context_get_source_info_by_index.
void PulseAudioInputStream::VolumeCallback(pa_context* context,
                                           const pa_source_info* info,
                                           int error, void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  if (error) {
    pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
    return;
  }

  if (stream->channels_ != info->channel_map.channels)
    stream->channels_ = info->channel_map.channels;

  pa_volume_t volume = PA_VOLUME_MUTED;  // Minimum possible value.
  // Use the max volume of any channel as the volume.
  for (int i = 0; i < stream->channels_; ++i) {
    if (volume < info->volume.values[i])
      volume = info->volume.values[i];
  }

  // It is safe to access |volume_| here since VolumeCallback() is running
  // under PulseLock.
  stream->volume_ = static_cast<double>(volume);
}

// static, used by pa_stream_set_state_callback.
void PulseAudioInputStream::StreamNotifyCallback(pa_stream* s,
                                                 void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);
  if (s && stream->callback_ &&
      pa_stream_get_state(s) == PA_STREAM_FAILED) {
    stream->callback_->OnError(stream);
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

void PulseAudioInputStream::ReadData() {
  uint32 hardware_delay = pulse::GetHardwareLatencyInBytes(
      handle_, params_.sample_rate(), params_.GetBytesPerFrame());

  // Update the AGC volume level once every second. Note that,
  // |volume| is also updated each time SetVolume() is called
  // through IPC by the render-side AGC.
  // We disregard the |normalized_volume| from GetAgcVolume()
  // and use the value calculated by |volume_|.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);
  normalized_volume = volume_ / GetMaxVolume();

  do {
    size_t length = 0;
    const void* data = NULL;
    pa_stream_peek(handle_, &data, &length);
    if (!data || length == 0)
      break;

    buffer_->Append(reinterpret_cast<const uint8*>(data), length);

    // Checks if we still have data.
    pa_stream_drop(handle_);
  } while (pa_stream_readable_size(handle_) > 0);

  int packet_size = params_.GetBytesPerBuffer();
  while (buffer_->forward_bytes() >= packet_size) {
    buffer_->Read(audio_data_buffer_.get(), packet_size);
    audio_bus_->FromInterleaved(audio_data_buffer_.get(),
                                audio_bus_->frames(),
                                params_.bits_per_sample() / 8);
    callback_->OnData(
        this, audio_bus_.get(), hardware_delay, normalized_volume);

    if (buffer_->forward_bytes() < packet_size)
      break;

    // TODO(xians): Remove once PPAPI is using circular buffers.
    DVLOG(1) << "OnData is being called consecutively, sleep 5ms to "
             << "wait until render consumes the data";
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(5));
  }

  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

}  // namespace media
