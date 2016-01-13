// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/base/limits.h"
#include "media/base/scoped_histogram_timer.h"
#include "media/base/user_input_monitor.h"

using base::TimeDelta;

namespace {
const int kMaxInputChannels = 3;

// TODO(henrika): remove usage of timers and add support for proper
// notification of when the input device is removed.  This was originally added
// to resolve http://crbug.com/79936 for Windows platforms.  This then caused
// breakage (very hard to repro bugs!) on other platforms: See
// http://crbug.com/226327 and http://crbug.com/230972.
// See also that the timer has been disabled on Mac now due to
// crbug.com/357501.
const int kTimerResetIntervalSeconds = 1;
// We have received reports that the timer can be too trigger happy on some
// Mac devices and the initial timer interval has therefore been increased
// from 1 second to 5 seconds.
const int kTimerInitialIntervalSeconds = 5;

#if defined(AUDIO_POWER_MONITORING)
// Time constant for AudioPowerMonitor.
// The utilized smoothing factor (alpha) in the exponential filter is given
// by 1-exp(-1/(fs*ts)), where fs is the sample rate in Hz and ts is the time
// constant given by |kPowerMeasurementTimeConstantMilliseconds|.
// Example: fs=44100, ts=10e-3 => alpha~0.022420
//          fs=44100, ts=20e-3 => alpha~0.165903
// A large smoothing factor corresponds to a faster filter response to input
// changes since y(n)=alpha*x(n)+(1-alpha)*y(n-1), where x(n) is the input
// and y(n) is the output.
const int kPowerMeasurementTimeConstantMilliseconds = 10;

// Time in seconds between two successive measurements of audio power levels.
const int kPowerMonitorLogIntervalSeconds = 5;
#endif
}

namespace media {

// static
AudioInputController::Factory* AudioInputController::factory_ = NULL;

AudioInputController::AudioInputController(EventHandler* handler,
                                           SyncWriter* sync_writer,
                                           UserInputMonitor* user_input_monitor)
    : creator_task_runner_(base::MessageLoopProxy::current()),
      handler_(handler),
      stream_(NULL),
      data_is_active_(false),
      state_(CLOSED),
      sync_writer_(sync_writer),
      max_volume_(0.0),
      user_input_monitor_(user_input_monitor),
      prev_key_down_count_(0) {
  DCHECK(creator_task_runner_.get());
}

AudioInputController::~AudioInputController() {
  DCHECK_EQ(state_, CLOSED);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    UserInputMonitor* user_input_monitor) {
  DCHECK(audio_manager);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  if (factory_) {
    return factory_->Create(
        audio_manager, event_handler, params, user_input_monitor);
  }
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, NULL, user_input_monitor));

  controller->task_runner_ = audio_manager->GetTaskRunner();

  // Create and open a new audio input stream from the existing
  // audio-device thread.
  if (!controller->task_runner_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateLowLatency(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    SyncWriter* sync_writer,
    UserInputMonitor* user_input_monitor) {
  DCHECK(audio_manager);
  DCHECK(sync_writer);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  // Create the AudioInputController object and ensure that it runs on
  // the audio-manager thread.
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, sync_writer, user_input_monitor));
  controller->task_runner_ = audio_manager->GetTaskRunner();

  // Create and open a new audio input stream from the existing
  // audio-device thread. Use the provided audio-input device.
  if (!controller->task_runner_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateForStream(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    EventHandler* event_handler,
    AudioInputStream* stream,
    SyncWriter* sync_writer,
    UserInputMonitor* user_input_monitor) {
  DCHECK(sync_writer);
  DCHECK(stream);

  // Create the AudioInputController object and ensure that it runs on
  // the audio-manager thread.
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, sync_writer, user_input_monitor));
  controller->task_runner_ = task_runner;

  // TODO(miu): See TODO at top of file.  Until that's resolved, we need to
  // disable the error auto-detection here (since the audio mirroring
  // implementation will reliably report error and close events).  Note, of
  // course, that we're assuming CreateForStream() has been called for the audio
  // mirroring use case only.
  if (!controller->task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&AudioInputController::DoCreateForStream, controller,
                     stream, false))) {
    controller = NULL;
  }

  return controller;
}

void AudioInputController::Record() {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoRecord, this));
}

void AudioInputController::Close(const base::Closure& closed_task) {
  DCHECK(!closed_task.is_null());
  DCHECK(creator_task_runner_->BelongsToCurrentThread());

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&AudioInputController::DoClose, this), closed_task);
}

void AudioInputController::SetVolume(double volume) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoSetVolume, this, volume));
}

void AudioInputController::SetAutomaticGainControl(bool enabled) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoSetAutomaticGainControl, this, enabled));
}

void AudioInputController::DoCreate(AudioManager* audio_manager,
                                    const AudioParameters& params,
                                    const std::string& device_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CreateTime");

#if defined(AUDIO_POWER_MONITORING)
  // Create the audio (power) level meter given the provided audio parameters.
  // An AudioBus is also needed to wrap the raw data buffer from the native
  // layer to match AudioPowerMonitor::Scan().
  // TODO(henrika): Remove use of extra AudioBus. See http://crbug.com/375155.
  audio_level_.reset(new media::AudioPowerMonitor(
      params.sample_rate(),
      TimeDelta::FromMilliseconds(kPowerMeasurementTimeConstantMilliseconds)));
  audio_params_ = params;
#endif

  // TODO(miu): See TODO at top of file.  Until that's resolved, assume all
  // platform audio input requires the |no_data_timer_| be used to auto-detect
  // errors.  In reality, probably only Windows needs to be treated as
  // unreliable here.
  DoCreateForStream(audio_manager->MakeAudioInputStream(params, device_id),
                    true);
}

void AudioInputController::DoCreateForStream(
    AudioInputStream* stream_to_control, bool enable_nodata_timer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!stream_);
  stream_ = stream_to_control;

  if (!stream_) {
    if (handler_)
      handler_->OnError(this, STREAM_CREATE_ERROR);
    return;
  }

  if (stream_ && !stream_->Open()) {
    stream_->Close();
    stream_ = NULL;
    if (handler_)
      handler_->OnError(this, STREAM_OPEN_ERROR);
    return;
  }

  DCHECK(!no_data_timer_.get());

  // The timer is enabled for logging purposes. The NO_DATA_ERROR triggered
  // from the timer must be ignored by the EventHandler.
  // TODO(henrika): remove usage of timer when it has been verified on Canary
  // that we are safe doing so. Goal is to get rid of |no_data_timer_| and
  // everything that is tied to it. crbug.com/357569.
  enable_nodata_timer = true;

  if (enable_nodata_timer) {
    // Create the data timer which will call FirstCheckForNoData(). The timer
    // is started in DoRecord() and restarted in each DoCheckForNoData()
    // callback.
    no_data_timer_.reset(new base::Timer(
        FROM_HERE, base::TimeDelta::FromSeconds(kTimerInitialIntervalSeconds),
        base::Bind(&AudioInputController::FirstCheckForNoData,
                   base::Unretained(this)), false));
  } else {
    DVLOG(1) << "Disabled: timer check for no data.";
  }

  state_ = CREATED;
  if (handler_)
    handler_->OnCreated(this);

  if (user_input_monitor_) {
    user_input_monitor_->EnableKeyPressMonitoring();
    prev_key_down_count_ = user_input_monitor_->GetKeyPressCount();
  }
}

void AudioInputController::DoRecord() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.RecordTime");

  if (state_ != CREATED)
    return;

  {
    base::AutoLock auto_lock(lock_);
    state_ = RECORDING;
  }

  if (no_data_timer_) {
    // Start the data timer. Once |kTimerResetIntervalSeconds| have passed,
    // a callback to FirstCheckForNoData() is made.
    no_data_timer_->Reset();
  }

  stream_->Start(this);
  if (handler_)
    handler_->OnRecording(this);
}

void AudioInputController::DoClose() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CloseTime");

  if (state_ == CLOSED)
    return;

  // Delete the timer on the same thread that created it.
  no_data_timer_.reset();

  DoStopCloseAndClearStream();
  SetDataIsActive(false);

  if (SharedMemoryAndSyncSocketMode())
    sync_writer_->Close();

  if (user_input_monitor_)
    user_input_monitor_->DisableKeyPressMonitoring();

  state_ = CLOSED;
}

void AudioInputController::DoReportError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnError(this, STREAM_ERROR);
}

void AudioInputController::DoSetVolume(double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1.0);

  if (state_ != CREATED && state_ != RECORDING)
    return;

  // Only ask for the maximum volume at first call and use cached value
  // for remaining function calls.
  if (!max_volume_) {
    max_volume_ = stream_->GetMaxVolume();
  }

  if (max_volume_ == 0.0) {
    DLOG(WARNING) << "Failed to access input volume control";
    return;
  }

  // Set the stream volume and scale to a range matched to the platform.
  stream_->SetVolume(max_volume_ * volume);
}

void AudioInputController::DoSetAutomaticGainControl(bool enabled) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, RECORDING);

  // Ensure that the AGC state only can be modified before streaming starts.
  if (state_ != CREATED)
    return;

  stream_->SetAutomaticGainControl(enabled);
}

void AudioInputController::FirstCheckForNoData() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  UMA_HISTOGRAM_BOOLEAN("Media.AudioInputControllerCaptureStartupSuccess",
                        GetDataIsActive());
  DoCheckForNoData();
}

void AudioInputController::DoCheckForNoData() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!GetDataIsActive()) {
    // The data-is-active marker will be false only if it has been more than
    // one second since a data packet was recorded. This can happen if a
    // capture device has been removed or disabled.
    if (handler_)
      handler_->OnError(this, NO_DATA_ERROR);
  }

  // Mark data as non-active. The flag will be re-enabled in OnData() each
  // time a data packet is received. Hence, under normal conditions, the
  // flag will only be disabled during a very short period.
  SetDataIsActive(false);

  // Restart the timer to ensure that we check the flag again in
  // |kTimerResetIntervalSeconds|.
  no_data_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kTimerResetIntervalSeconds),
      base::Bind(&AudioInputController::DoCheckForNoData,
      base::Unretained(this)));
}

void AudioInputController::OnData(AudioInputStream* stream,
                                  const AudioBus* source,
                                  uint32 hardware_delay_bytes,
                                  double volume) {
  // Mark data as active to ensure that the periodic calls to
  // DoCheckForNoData() does not report an error to the event handler.
  SetDataIsActive(true);

  {
    base::AutoLock auto_lock(lock_);
    if (state_ != RECORDING)
      return;
  }

  bool key_pressed = false;
  if (user_input_monitor_) {
    size_t current_count = user_input_monitor_->GetKeyPressCount();
    key_pressed = current_count != prev_key_down_count_;
    prev_key_down_count_ = current_count;
    DVLOG_IF(6, key_pressed) << "Detected keypress.";
  }

  // Use SharedMemory and SyncSocket if the client has created a SyncWriter.
  // Used by all low-latency clients except WebSpeech.
  if (SharedMemoryAndSyncSocketMode()) {
    sync_writer_->Write(source, volume, key_pressed);
    sync_writer_->UpdateRecordedBytes(hardware_delay_bytes);

#if defined(AUDIO_POWER_MONITORING)
    // Only do power-level measurements if an AudioPowerMonitor object has
    // been created. Done in DoCreate() but not DoCreateForStream(), hence
    // logging will mainly be done for WebRTC and WebSpeech clients.
    if (!audio_level_)
      return;

    // Perform periodic audio (power) level measurements.
    if ((base::TimeTicks::Now() - last_audio_level_log_time_).InSeconds() >
        kPowerMonitorLogIntervalSeconds) {
      // Wrap data into an AudioBus to match AudioPowerMonitor::Scan.
      // TODO(henrika): remove this section when capture side uses AudioBus.
      // See http://crbug.com/375155 for details.
      audio_level_->Scan(*source, source->frames());

      // Get current average power level and add it to the log.
      // Possible range is given by [-inf, 0] dBFS.
      std::pair<float, bool> result = audio_level_->ReadCurrentPowerAndClip();

      // Use event handler on the audio thread to relay a message to the ARIH
      // in content which does the actual logging on the IO thread.
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &AudioInputController::DoLogAudioLevel, this, result.first));

      last_audio_level_log_time_ = base::TimeTicks::Now();

      // Reset the average power level (since we don't log continuously).
      audio_level_->Reset();
    }
#endif
    return;
  }

  // TODO(henrika): Investigate if we can avoid the extra copy here.
  // (see http://crbug.com/249316 for details). AFAIK, this scope is only
  // active for WebSpeech clients.
  scoped_ptr<AudioBus> audio_data =
      AudioBus::Create(source->channels(), source->frames());
  source->CopyTo(audio_data.get());

  // Ownership of the audio buffer will be with the callback until it is run,
  // when ownership is passed to the callback function.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &AudioInputController::DoOnData, this, base::Passed(&audio_data)));
}

void AudioInputController::DoOnData(scoped_ptr<AudioBus> data) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnData(this, data.get());
}

void AudioInputController::DoLogAudioLevel(float level_dbfs) {
#if defined(AUDIO_POWER_MONITORING)
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!handler_)
    return;

  std::string log_string = base::StringPrintf(
      "AIC::OnData: average audio level=%.2f dBFS", level_dbfs);
  static const float kSilenceThresholdDBFS = -72.24719896f;
  if (level_dbfs < kSilenceThresholdDBFS)
    log_string += " <=> no audio input!";

  handler_->OnLog(this, log_string);
#endif
}

void AudioInputController::OnError(AudioInputStream* stream) {
  // Handle error on the audio-manager thread.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoReportError, this));
}

void AudioInputController::DoStopCloseAndClearStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (stream_ != NULL) {
    stream_->Stop();
    stream_->Close();
    stream_ = NULL;
  }

  // The event handler should not be touched after the stream has been closed.
  handler_ = NULL;
}

void AudioInputController::SetDataIsActive(bool enabled) {
  base::subtle::Release_Store(&data_is_active_, enabled);
}

bool AudioInputController::GetDataIsActive() {
  return (base::subtle::Acquire_Load(&data_is_active_) != false);
}

}  // namespace media
