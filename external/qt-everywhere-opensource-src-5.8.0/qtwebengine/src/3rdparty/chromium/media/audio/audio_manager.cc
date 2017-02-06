// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_switches.h"

#if defined(OS_MACOSX)
#include "media/audio/mac/audio_manager_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace media {
namespace {

// The singleton instance of AudioManager. This is set when Create() is called.
AudioManager* g_last_created = nullptr;

// Maximum number of failed pings to the audio thread allowed. A UMA will be
// recorded once this count is reached; if enabled, a non-crash dump will be
// captured as well. We require at least three failed pings before recording to
// ensure unobservable power events aren't mistakenly caught (e.g., the system
// suspends before a OnSuspend() event can be fired).
const int kMaxFailedPingsCount = 3;

// Helper class for managing global AudioManager data and hang monitor. If the
// audio thread is hung for > |kMaxFailedPingsCount| * |max_hung_task_time_|, we
// want to record a UMA and optionally a non-crash dump to find offenders in the
// field.
class AudioManagerHelper : public base::PowerObserver {
 public:
  // These values are histogrammed over time; do not change their ordinal
  // values.
  enum ThreadStatus {
    THREAD_NONE = 0,
    THREAD_STARTED,
    THREAD_HUNG,
    THREAD_RECOVERED,
    THREAD_MAX = THREAD_RECOVERED
  };

  AudioManagerHelper() {}
  ~AudioManagerHelper() override {}

  void StartHangTimer(
      scoped_refptr<base::SingleThreadTaskRunner> monitor_task_runner) {
    CHECK(!monitor_task_runner_);
    CHECK(!audio_task_runner_);
    monitor_task_runner_ = std::move(monitor_task_runner);
    audio_task_runner_ = AudioManager::Get()->GetTaskRunner();
    base::PowerMonitor::Get()->AddObserver(this);

    io_task_running_ = audio_task_running_ = true;
    audio_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                   base::Unretained(this)));
    monitor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&AudioManagerHelper::RecordAudioThreadStatus,
                              base::Unretained(this)));
  }

  AudioLogFactory* fake_log_factory() { return &fake_log_factory_; }

#if defined(OS_WIN)
  // This should be called before creating an AudioManager in tests to ensure
  // that the creating thread is COM initialized.
  void InitializeCOMForTesting() {
    com_initializer_for_testing_.reset(new base::win::ScopedCOMInitializer());
  }
#endif

#if defined(OS_LINUX)
  void set_app_name(const std::string& app_name) { app_name_ = app_name; }
  const std::string& app_name() const { return app_name_; }
#endif

  void enable_crash_key_logging() { enable_crash_key_logging_ = true; }

 private:
  // base::PowerObserver overrides.
  // Disable hang detection when the system goes into the suspend state.
  void OnSuspend() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = false;
    failed_pings_ = successful_pings_ = 0;
  }
  // Reenable hang detection once the system comes out of the suspend state.
  void OnResume() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = true;
    last_audio_thread_timer_tick_ = base::TimeTicks::Now();
    failed_pings_ = successful_pings_ = 0;

    // If either of the tasks were stopped during suspend, start them now.
    if (!audio_task_running_) {
      audio_task_running_ = true;

      base::AutoUnlock unlock(hang_lock_);
      audio_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                     base::Unretained(this)));
    }

    if (!io_task_running_) {
      io_task_running_ = true;

      base::AutoUnlock unlock(hang_lock_);
      monitor_task_runner_->PostTask(
          FROM_HERE, base::Bind(&AudioManagerHelper::RecordAudioThreadStatus,
                                base::Unretained(this)));
    }
  }

  // Runs on |monitor_task_runner|.
  void RecordAudioThreadStatus() {
    DCHECK(monitor_task_runner_->BelongsToCurrentThread());
    {
      base::AutoLock lock(hang_lock_);

      // Don't attempt to verify the tick time or post our task if the system is
      // in the process of suspending or resuming.
      if (!hang_detection_enabled_) {
        io_task_running_ = false;
        return;
      }

      DCHECK(io_task_running_);
      const base::TimeTicks now = base::TimeTicks::Now();
      const base::TimeDelta tick_delta = now - last_audio_thread_timer_tick_;
      if (tick_delta > max_hung_task_time_) {
        successful_pings_ = 0;
        if (++failed_pings_ >= kMaxFailedPingsCount &&
            audio_thread_status_ < THREAD_HUNG) {
          if (enable_crash_key_logging_)
            LogAudioDriverCrashKeys();
          HistogramThreadStatus(THREAD_HUNG);
        }
      } else {
        failed_pings_ = 0;
        ++successful_pings_;
        if (audio_thread_status_ == THREAD_NONE) {
          HistogramThreadStatus(THREAD_STARTED);
        } else if (audio_thread_status_ == THREAD_HUNG &&
                   successful_pings_ >= kMaxFailedPingsCount) {
          // Require just as many successful pings to recover from failure.
          HistogramThreadStatus(THREAD_RECOVERED);
        }
      }
    }

    // Don't hold the lock while posting the next task.
    monitor_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&AudioManagerHelper::RecordAudioThreadStatus,
                              base::Unretained(this)),
        max_hung_task_time_);
  }

  // Runs on the audio thread.
  void UpdateLastAudioThreadTimeTick() {
    DCHECK(audio_task_runner_->BelongsToCurrentThread());
    {
      base::AutoLock lock(hang_lock_);
      last_audio_thread_timer_tick_ = base::TimeTicks::Now();
      failed_pings_ = 0;

      // Don't post our task if the system is or will be suspended.
      if (!hang_detection_enabled_) {
        audio_task_running_ = false;
        return;
      }

      DCHECK(audio_task_running_);
    }

    // Don't hold the lock while posting the next task.
    audio_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                   base::Unretained(this)),
        max_hung_task_time_ / 5);
  }

  void HistogramThreadStatus(ThreadStatus status) {
    DCHECK(monitor_task_runner_->BelongsToCurrentThread());
    audio_thread_status_ = status;
    UMA_HISTOGRAM_ENUMERATION("Media.AudioThreadStatus", audio_thread_status_,
                              THREAD_MAX + 1);
  }

  void LogAudioDriverCrashKeys() {
    DCHECK(monitor_task_runner_->BelongsToCurrentThread());
    DCHECK(enable_crash_key_logging_);

#if defined(OS_WIN)
    std::string driver_name, driver_version;
    if (!CoreAudioUtil::GetDxDiagDetails(&driver_name, &driver_version))
      return;

    base::debug::ScopedCrashKey crash_key(
        "hung-audio-thread-details",
        base::StringPrintf("%s:%s", driver_name.c_str(),
                           driver_version.c_str()));

    // Please forward crash reports to http://crbug.com/422522
    base::debug::DumpWithoutCrashing();
#endif
  }

  FakeAudioLogFactory fake_log_factory_;

  const base::TimeDelta max_hung_task_time_ = base::TimeDelta::FromMinutes(1);
  scoped_refptr<base::SingleThreadTaskRunner> monitor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  base::Lock hang_lock_;
  bool hang_detection_enabled_ = true;
  base::TimeTicks last_audio_thread_timer_tick_;
  uint32_t failed_pings_ = 0;
  bool io_task_running_ = false;
  bool audio_task_running_ = false;
  ThreadStatus audio_thread_status_ = THREAD_NONE;
  bool enable_crash_key_logging_ = false;
  uint32_t successful_pings_ = 0;

#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_for_testing_;
#endif

#if defined(OS_LINUX)
  std::string app_name_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AudioManagerHelper);
};

base::LazyInstance<AudioManagerHelper>::Leaky g_helper =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void AudioManagerDeleter::operator()(const AudioManager* instance) const {
  CHECK(instance);
  // We reset g_last_created here instead of in the destructor of AudioManager
  // because the destructor runs on the audio thread. We want to always change
  // g_last_created from the main thread.
  if (g_last_created == instance) {
    g_last_created = nullptr;
  } else {
    // We create multiple instances of AudioManager only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioManager detected";
  }

#if defined(OS_MACOSX)
  // If we are on Mac, tasks after this point are not executed, hence this is
  // the only chance to delete the audio manager (which on Mac lives on the
  // main browser thread instead of a dedicated audio thread). If we don't
  // delete here, the CoreAudio thread can keep providing callbacks, which
  // uses a state that is destroyed in ~BrowserMainLoop().
  // See http://crbug.com/623703 for more details.
  DCHECK(instance->GetTaskRunner()->BelongsToCurrentThread());
  AudioManagerMac* mac_instance =
      static_cast<AudioManagerMac*>(const_cast<AudioManager*>(instance));
  delete mac_instance;
#else
  // AudioManager must be destroyed on the audio thread.
  if (!instance->GetTaskRunner()->DeleteSoon(FROM_HERE, instance)) {
    LOG(WARNING) << "Failed to delete AudioManager instance.";
  }
#endif
}

// Forward declaration of the platform specific AudioManager factory function.
ScopedAudioManagerPtr CreateAudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory);

AudioManager::AudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner)
    : task_runner_(std::move(task_runner)),
      worker_task_runner_(std::move(worker_task_runner)) {
  DCHECK(task_runner_);
  DCHECK(worker_task_runner_);

  if (g_last_created) {
    // We create multiple instances of AudioManager only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioManager detected";
  }
  // We always override |g_last_created| irrespective of whether it is already
  // set or not becuase it represents the last created instance.
  g_last_created = this;
}

AudioManager::~AudioManager() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

// static
ScopedAudioManagerPtr AudioManager::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory) {
  DCHECK(task_runner);
  DCHECK(worker_task_runner);
  return CreateAudioManager(std::move(task_runner),
                            std::move(worker_task_runner), audio_log_factory);
}

// static
ScopedAudioManagerPtr AudioManager::CreateForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if defined(OS_WIN)
  g_helper.Pointer()->InitializeCOMForTesting();
#endif
  return Create(task_runner, task_runner,
                g_helper.Pointer()->fake_log_factory());
}

// static
void AudioManager::StartHangMonitor(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(AudioManager::Get());
  DCHECK(task_runner);
  DCHECK_NE(task_runner, AudioManager::Get()->GetTaskRunner());

  g_helper.Pointer()->StartHangTimer(std::move(task_runner));
}

// static
void AudioManager::EnableCrashKeyLoggingForAudioThreadHangs() {
  CHECK(!g_last_created);
  g_helper.Pointer()->enable_crash_key_logging();
}

#if defined(OS_LINUX)
// static
void AudioManager::SetGlobalAppName(const std::string& app_name) {
  g_helper.Pointer()->set_app_name(app_name);
}

// static
const std::string& AudioManager::GetGlobalAppName() {
  return g_helper.Pointer()->app_name();
}
#endif

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

}  // namespace media
