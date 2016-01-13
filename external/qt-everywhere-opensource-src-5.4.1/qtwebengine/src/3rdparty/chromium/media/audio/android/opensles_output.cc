// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_output.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/audio/android/audio_manager_android.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...)      \
  do {                                          \
    SLresult err = (op);                        \
    if (err != SL_RESULT_SUCCESS) {             \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__;                       \
    }                                           \
  } while (0)

namespace media {

OpenSLESOutputStream::OpenSLESOutputStream(AudioManagerAndroid* manager,
                                           const AudioParameters& params,
                                           SLint32 stream_type)
    : audio_manager_(manager),
      stream_type_(stream_type),
      callback_(NULL),
      player_(NULL),
      simple_buffer_queue_(NULL),
      active_buffer_index_(0),
      buffer_size_bytes_(0),
      started_(false),
      muted_(false),
      volume_(1.0) {
  DVLOG(2) << "OpenSLESOutputStream::OpenSLESOutputStream("
           << "stream_type=" << stream_type << ")";
  format_.formatType = SL_DATAFORMAT_PCM;
  format_.numChannels = static_cast<SLuint32>(params.channels());
  // Provides sampling rate in milliHertz to OpenSLES.
  format_.samplesPerSec = static_cast<SLuint32>(params.sample_rate() * 1000);
  format_.bitsPerSample = params.bits_per_sample();
  format_.containerSize = params.bits_per_sample();
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  if (format_.numChannels == 1)
    format_.channelMask = SL_SPEAKER_FRONT_CENTER;
  else if (format_.numChannels == 2)
    format_.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  else
    NOTREACHED() << "Unsupported number of channels: " << format_.numChannels;

  buffer_size_bytes_ = params.GetBytesPerBuffer();
  audio_bus_ = AudioBus::Create(params);

  memset(&audio_data_, 0, sizeof(audio_data_));
}

OpenSLESOutputStream::~OpenSLESOutputStream() {
  DVLOG(2) << "OpenSLESOutputStream::~OpenSLESOutputStream()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!player_object_.Get());
  DCHECK(!output_mixer_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

bool OpenSLESOutputStream::Open() {
  DVLOG(2) << "OpenSLESOutputStream::Open()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_object_.Get())
    return false;

  if (!CreatePlayer())
    return false;

  SetupAudioBuffer();
  active_buffer_index_ = 0;

  return true;
}

void OpenSLESOutputStream::Start(AudioSourceCallback* callback) {
  DVLOG(2) << "OpenSLESOutputStream::Start()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(player_);
  DCHECK(simple_buffer_queue_);
  if (started_)
    return;

  base::AutoLock lock(lock_);
  DCHECK(callback_ == NULL || callback_ == callback);
  callback_ = callback;

  // Avoid start-up glitches by filling up one buffer queue before starting
  // the stream.
  FillBufferQueueNoLock();

  // Start streaming data by setting the play state to SL_PLAYSTATE_PLAYING.
  // For a player object, when the object is in the SL_PLAYSTATE_PLAYING
  // state, adding buffers will implicitly start playback.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING));

  started_ = true;
}

void OpenSLESOutputStream::Stop() {
  DVLOG(2) << "OpenSLESOutputStream::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!started_)
    return;

  base::AutoLock lock(lock_);

  // Stop playing by setting the play state to SL_PLAYSTATE_STOPPED.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED));

  // Clear the buffer queue so that the old data won't be played when
  // resuming playing.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->Clear(simple_buffer_queue_));

#ifndef NDEBUG
  // Verify that the buffer queue is in fact cleared as it should.
  SLAndroidSimpleBufferQueueState buffer_queue_state;
  LOG_ON_FAILURE_AND_RETURN((*simple_buffer_queue_)->GetState(
      simple_buffer_queue_, &buffer_queue_state));
  DCHECK_EQ(0u, buffer_queue_state.count);
  DCHECK_EQ(0u, buffer_queue_state.index);
#endif

  callback_ = NULL;
  started_ = false;
}

void OpenSLESOutputStream::Close() {
  DVLOG(2) << "OpenSLESOutputStream::Close()";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop the stream if it is still playing.
  Stop();
  {
    // Destroy the buffer queue player object and invalidate all associated
    // interfaces.
    player_object_.Reset();
    simple_buffer_queue_ = NULL;
    player_ = NULL;

    // Destroy the mixer object. We don't store any associated interface for
    // this object.
    output_mixer_.Reset();

    // Destroy the engine object. We don't store any associated interface for
    // this object.
    engine_object_.Reset();
    ReleaseAudioBuffer();
  }

  audio_manager_->ReleaseOutputStream(this);
}

void OpenSLESOutputStream::SetVolume(double volume) {
  DVLOG(2) << "OpenSLESOutputStream::SetVolume(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void OpenSLESOutputStream::GetVolume(double* volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *volume = static_cast<double>(volume_);
}

void OpenSLESOutputStream::SetMute(bool muted) {
  DVLOG(2) << "OpenSLESOutputStream::SetMute(" << muted << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  muted_ = muted;
}

bool OpenSLESOutputStream::CreatePlayer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!player_object_.Get());
  DCHECK(!output_mixer_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);

  // Initializes the engine object with specific option. After working with the
  // object, we need to free the object and its resources.
  SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  LOG_ON_FAILURE_AND_RETURN(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL),
      false);

  // Realize the SL engine object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE), false);

  // Get the SL engine interface which is implicit.
  SLEngineItf engine;
  LOG_ON_FAILURE_AND_RETURN(engine_object_->GetInterface(
                                engine_object_.Get(), SL_IID_ENGINE, &engine),
                            false);

  // Create ouput mixer object to be used by the player.
  LOG_ON_FAILURE_AND_RETURN((*engine)->CreateOutputMix(
                                engine, output_mixer_.Receive(), 0, NULL, NULL),
                            false);

  // Realizing the output mix object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      output_mixer_->Realize(output_mixer_.Get(), SL_BOOLEAN_FALSE), false);

  // Audio source configuration.
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<SLuint32>(kMaxNumOfBuffersInQueue)};
  SLDataSource audio_source = {&simple_buffer_queue, &format_};

  // Audio sink configuration.
  SLDataLocator_OutputMix locator_output_mix = {SL_DATALOCATOR_OUTPUTMIX,
                                                output_mixer_.Get()};
  SLDataSink audio_sink = {&locator_output_mix, NULL};

  // Create an audio player.
  const SLInterfaceID interface_id[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME,
                                        SL_IID_ANDROIDCONFIGURATION};
  const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                                          SL_BOOLEAN_TRUE};
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateAudioPlayer(engine,
                                   player_object_.Receive(),
                                   &audio_source,
                                   &audio_sink,
                                   arraysize(interface_id),
                                   interface_id,
                                   interface_required),
      false);

  // Create AudioPlayer and specify SL_IID_ANDROIDCONFIGURATION.
  SLAndroidConfigurationItf player_config;
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(
          player_object_.Get(), SL_IID_ANDROIDCONFIGURATION, &player_config),
      false);

  // Set configuration using the stream type provided at construction.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_config)->SetConfiguration(player_config,
                                         SL_ANDROID_KEY_STREAM_TYPE,
                                         &stream_type_,
                                         sizeof(SLint32)),
      false);

  // Realize the player object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->Realize(player_object_.Get(), SL_BOOLEAN_FALSE), false);

  // Get an implicit player interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(player_object_.Get(), SL_IID_PLAY, &player_),
      false);

  // Get the simple buffer queue interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(
          player_object_.Get(), SL_IID_BUFFERQUEUE, &simple_buffer_queue_),
      false);

  // Register the input callback for the simple buffer queue.
  // This callback will be called when the soundcard needs data.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->RegisterCallback(
          simple_buffer_queue_, SimpleBufferQueueCallback, this),
      false);

  return true;
}

void OpenSLESOutputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue,
    void* instance) {
  OpenSLESOutputStream* stream =
      reinterpret_cast<OpenSLESOutputStream*>(instance);
  stream->FillBufferQueue();
}

void OpenSLESOutputStream::FillBufferQueue() {
  base::AutoLock lock(lock_);
  if (!started_)
    return;

  TRACE_EVENT0("audio", "OpenSLESOutputStream::FillBufferQueue");

  // Verify that we are in a playing state.
  SLuint32 state;
  SLresult err = (*player_)->GetPlayState(player_, &state);
  if (SL_RESULT_SUCCESS != err) {
    HandleError(err);
    return;
  }
  if (state != SL_PLAYSTATE_PLAYING) {
    DLOG(WARNING) << "Received callback in non-playing state";
    return;
  }

  // Fill up one buffer in the queue by asking the registered source for
  // data using the OnMoreData() callback.
  FillBufferQueueNoLock();
}

void OpenSLESOutputStream::FillBufferQueueNoLock() {
  // Ensure that the calling thread has acquired the lock since it is not
  // done in this method.
  lock_.AssertAcquired();

  // Read data from the registered client source.
  // TODO(henrika): Investigate if it is possible to get a more accurate
  // delay estimation.
  const uint32 hardware_delay = buffer_size_bytes_;
  int frames_filled = callback_->OnMoreData(
      audio_bus_.get(), AudioBuffersState(0, hardware_delay));
  if (frames_filled <= 0) {
    // Audio source is shutting down, or halted on error.
    return;
  }

  // Note: If the internal representation ever changes from 16-bit PCM to
  // raw float, the data must be clipped and sanitized since it may come
  // from an untrusted source such as NaCl.
  audio_bus_->Scale(muted_ ? 0.0f : volume_);
  audio_bus_->ToInterleaved(frames_filled,
                            format_.bitsPerSample / 8,
                            audio_data_[active_buffer_index_]);

  const int num_filled_bytes =
      frames_filled * audio_bus_->channels() * format_.bitsPerSample / 8;
  DCHECK_LE(static_cast<size_t>(num_filled_bytes), buffer_size_bytes_);

  // Enqueue the buffer for playback.
  SLresult err =
      (*simple_buffer_queue_)->Enqueue(simple_buffer_queue_,
                                       audio_data_[active_buffer_index_],
                                       num_filled_bytes);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
}

void OpenSLESOutputStream::SetupAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    audio_data_[i] = new uint8[buffer_size_bytes_];
  }
}

void OpenSLESOutputStream::ReleaseAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_data_[0]) {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      delete[] audio_data_[i];
      audio_data_[i] = NULL;
    }
  }
}

void OpenSLESOutputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "OpenSLES Output error " << error;
  if (callback_)
    callback_->OnError(this);
}

}  // namespace media
