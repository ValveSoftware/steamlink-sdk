// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_output.h"

#include <pulse/pulseaudio.h>

#include "base/single_thread_task_runner.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/pulse/pulse_util.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// static, pa_stream_notify_cb
void PulseAudioOutputStream::StreamNotifyCallback(pa_stream* s, void* p_this) {
  PulseAudioOutputStream* stream = static_cast<PulseAudioOutputStream*>(p_this);

  // Forward unexpected failures to the AudioSourceCallback if available.  All
  // these variables are only modified under pa_threaded_mainloop_lock() so this
  // should be thread safe.
  if (s && stream->source_callback_ &&
      pa_stream_get_state(s) == PA_STREAM_FAILED) {
    stream->source_callback_->OnError(stream);
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

// static, pa_stream_request_cb_t
void PulseAudioOutputStream::StreamRequestCallback(pa_stream* s, size_t len,
                                                   void* p_this) {
  // Fulfill write request; must always result in a pa_stream_write() call.
  static_cast<PulseAudioOutputStream*>(p_this)->FulfillWriteRequest(len);
}

PulseAudioOutputStream::PulseAudioOutputStream(const AudioParameters& params,
                                               const std::string& device_id,
                                               AudioManagerBase* manager)
    : params_(params),
      device_id_(device_id),
      manager_(manager),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      pa_stream_(NULL),
      volume_(1.0f),
      source_callback_(NULL) {
  CHECK(params_.IsValid());
  audio_bus_ = AudioBus::Create(params_);
}

PulseAudioOutputStream::~PulseAudioOutputStream() {
  // All internal structures should already have been freed in Close(), which
  // calls AudioManagerBase::ReleaseOutputStream() which deletes this object.
  DCHECK(!pa_stream_);
  DCHECK(!pa_context_);
  DCHECK(!pa_mainloop_);
}

bool PulseAudioOutputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return pulse::CreateOutputStream(&pa_mainloop_, &pa_context_, &pa_stream_,
                                   params_, device_id_, &StreamNotifyCallback,
                                   &StreamRequestCallback, this);
}

void PulseAudioOutputStream::Reset() {
  if (!pa_mainloop_) {
    DCHECK(!pa_stream_);
    DCHECK(!pa_context_);
    return;
  }

  {
    AutoPulseLock auto_lock(pa_mainloop_);

    // Close the stream.
    if (pa_stream_) {
      // Ensure all samples are played out before shutdown.
      pa_operation* operation = pa_stream_flush(
          pa_stream_, &pulse::StreamSuccessCallback, pa_mainloop_);
      WaitForOperationCompletion(pa_mainloop_, operation);

      // Release PulseAudio structures.
      pa_stream_disconnect(pa_stream_);
      pa_stream_set_write_callback(pa_stream_, NULL, NULL);
      pa_stream_set_state_callback(pa_stream_, NULL, NULL);
      pa_stream_unref(pa_stream_);
      pa_stream_ = NULL;
    }

    if (pa_context_) {
      pa_context_disconnect(pa_context_);
      pa_context_set_state_callback(pa_context_, NULL, NULL);
      pa_context_unref(pa_context_);
      pa_context_ = NULL;
    }
  }

  pa_threaded_mainloop_stop(pa_mainloop_);
  pa_threaded_mainloop_free(pa_mainloop_);
  pa_mainloop_ = NULL;
}

void PulseAudioOutputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Reset();

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void PulseAudioOutputStream::FulfillWriteRequest(size_t requested_bytes) {
  int bytes_remaining = requested_bytes;
  while (bytes_remaining > 0) {
    void* buffer = NULL;
    size_t bytes_to_fill = params_.GetBytesPerBuffer();
    CHECK_GE(pa_stream_begin_write(pa_stream_, &buffer, &bytes_to_fill), 0);
    CHECK_EQ(bytes_to_fill, static_cast<size_t>(params_.GetBytesPerBuffer()));

    // NOTE: |bytes_to_fill| may be larger than |requested_bytes| now, this is
    // okay since pa_stream_begin_write() is the authoritative source on how
    // much can be written.

    int frames_filled = 0;
    if (source_callback_) {
      const uint32 hardware_delay = pulse::GetHardwareLatencyInBytes(
          pa_stream_, params_.sample_rate(), params_.GetBytesPerFrame());
      frames_filled = source_callback_->OnMoreData(
          audio_bus_.get(), AudioBuffersState(0, hardware_delay));

      // Zero any unfilled data so it plays back as silence.
      if (frames_filled < audio_bus_->frames()) {
        audio_bus_->ZeroFramesPartial(
            frames_filled, audio_bus_->frames() - frames_filled);
      }

      // Note: If this ever changes to output raw float the data must be clipped
      // and sanitized since it may come from an untrusted source such as NaCl.
      audio_bus_->Scale(volume_);
      audio_bus_->ToInterleaved(
          audio_bus_->frames(), params_.bits_per_sample() / 8, buffer);
    } else {
      memset(buffer, 0, bytes_to_fill);
    }

    if (pa_stream_write(pa_stream_, buffer, bytes_to_fill, NULL, 0LL,
                        PA_SEEK_RELATIVE) < 0) {
      if (source_callback_) {
        source_callback_->OnError(this);
      }
    }

    // NOTE: As mentioned above, |bytes_remaining| may be negative after this.
    bytes_remaining -= bytes_to_fill;

    // Despite telling Pulse to only request certain buffer sizes, it will not
    // always obey.  In these cases we need to avoid back to back reads from
    // the renderer as it won't have time to complete the request.
    //
    // We can't defer the callback as Pulse will never call us again until we've
    // satisfied writing the requested number of bytes.
    //
    // TODO(dalecurtis): It might be worth choosing the sleep duration based on
    // the hardware latency return above.  Watch http://crbug.com/366433 to see
    // if a more complicated wait process is necessary.  We may also need to see
    // if a PostDelayedTask should be used here to avoid blocking the PulseAudio
    // command thread.
    if (source_callback_ && bytes_remaining > 0)
      base::PlatformThread::Sleep(params_.GetBufferDuration() / 4);
  }
}

void PulseAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(callback);
  CHECK(pa_stream_);

  AutoPulseLock auto_lock(pa_mainloop_);

  // Ensure the context and stream are ready.
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY &&
      pa_stream_get_state(pa_stream_) != PA_STREAM_READY) {
    callback->OnError(this);
    return;
  }

  source_callback_ = callback;

  // Uncork (resume) the stream.
  pa_operation* operation = pa_stream_cork(
      pa_stream_, 0, &pulse::StreamSuccessCallback, pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);
}

void PulseAudioOutputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cork (pause) the stream.  Waiting for the main loop lock will ensure
  // outstanding callbacks have completed.
  AutoPulseLock auto_lock(pa_mainloop_);

  // Set |source_callback_| to NULL so all FulfillWriteRequest() calls which may
  // occur while waiting on the flush and cork exit immediately.
  source_callback_ = NULL;

  // Flush the stream prior to cork, doing so after will cause hangs.  Write
  // callbacks are suspended while inside pa_threaded_mainloop_lock() so this
  // is all thread safe.
  pa_operation* operation = pa_stream_flush(
      pa_stream_, &pulse::StreamSuccessCallback, pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);

  operation = pa_stream_cork(pa_stream_, 1, &pulse::StreamSuccessCallback,
                             pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);
}

void PulseAudioOutputStream::SetVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());

  volume_ = static_cast<float>(volume);
}

void PulseAudioOutputStream::GetVolume(double* volume) {
  DCHECK(thread_checker_.CalledOnValidThread());

  *volume = volume_;
}

}  // namespace media
