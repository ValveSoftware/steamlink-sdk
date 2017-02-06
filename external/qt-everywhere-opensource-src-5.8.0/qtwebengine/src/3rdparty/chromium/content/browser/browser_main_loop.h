// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#define CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/public/browser/browser_main_runner.h"
#include "media/audio/audio_manager.h"

#if defined(USE_AURA)
namespace aura {
class Env;
}
#endif

namespace base {
class CommandLine;
class FilePath;
class HighResolutionTimerManager;
class MessageLoop;
class PowerMonitor;
class SystemMonitor;
class MemoryPressureMonitor;
namespace trace_event {
class TraceEventSystemStatsMonitor;
}  // namespace trace_event
}  // namespace base

namespace media {
#if defined(OS_WIN)
class SystemMessageWindowWin;
#elif defined(OS_LINUX) && defined(USE_UDEV)
class DeviceMonitorLinux;
#endif
class UserInputMonitor;
#if defined(OS_MACOSX)
class DeviceMonitorMac;
#endif
namespace midi {
class MidiManager;
}  // namespace midi
}  // namespace media

namespace mojo {
namespace edk {
class ScopedIPCSupport;
}  // namespace edk
}  // namespace mojo

namespace net {
class NetworkChangeNotifier;
}  // namespace net

#if defined(USE_OZONE)
namespace ui {
class ClientNativePixmapFactory;
}  // namespace ui
#endif

namespace content {
class BrowserMainParts;
class BrowserOnlineStateObserver;
class BrowserThreadImpl;
class LoaderDelegateImpl;
class MediaStreamManager;
class MojoShellContext;
class ResourceDispatcherHostImpl;
class SpeechRecognitionManagerImpl;
class StartupTaskRunner;
class TimeZoneMonitor;
struct MainFunctionParams;

#if defined(OS_ANDROID)
class ScreenOrientationDelegate;
#elif defined(OS_WIN)
class ScreenOrientationDelegate;
#endif

// Implements the main browser loop stages called from BrowserMainRunner.
// See comments in browser_main_parts.h for additional info.
class CONTENT_EXPORT BrowserMainLoop {
 public:
  // Returns the current instance. This is used to get access to the getters
  // that return objects which are owned by this class.
  static BrowserMainLoop* GetInstance();

  explicit BrowserMainLoop(const MainFunctionParams& parameters);
  virtual ~BrowserMainLoop();

  void Init();

  void EarlyInitialization();

  // Initializes the toolkit. Returns whether the toolkit initialization was
  // successful or not.
  bool InitializeToolkit();

  void PreMainMessageLoopStart();
  void MainMessageLoopStart();
  void PostMainMessageLoopStart();

  // Create and start running the tasks we need to complete startup. Note that
  // this can be called more than once (currently only on Android) if we get a
  // request for synchronous startup while the tasks created by asynchronous
  // startup are still running.
  void CreateStartupTasks();

  // Perform the default message loop run logic.
  void RunMainMessageLoopParts();

  // Performs the shutdown sequence, starting with PostMainMessageLoopRun
  // through stopping threads to PostDestroyThreads.
  void ShutdownThreadsAndCleanUp();

  int GetResultCode() const { return result_code_; }

  media::AudioManager* audio_manager() const { return audio_manager_.get(); }
  MediaStreamManager* media_stream_manager() const {
    return media_stream_manager_.get();
  }
  media::UserInputMonitor* user_input_monitor() const {
    return user_input_monitor_.get();
  }
  media::midi::MidiManager* midi_manager() const { return midi_manager_.get(); }
  base::Thread* indexed_db_thread() const { return indexed_db_thread_.get(); }

  bool is_tracing_startup_for_duration() const {
    return is_tracing_startup_for_duration_;
  }

  const base::FilePath& startup_trace_file() const {
    return startup_trace_file_;
  }

  void StopStartupTracingTimer();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  media::DeviceMonitorMac* device_monitor_mac() const {
    return device_monitor_mac_.get();
  }
#endif

 private:
  class MemoryObserver;

  void InitializeMainThread();

  // Called just before creating the threads
  int PreCreateThreads();

  // Create all secondary threads.
  int CreateThreads();

  // Called right after the browser threads have been started.
  int BrowserThreadsStarted();

  int PreMainMessageLoopRun();

  void MainMessageLoopRun();

  base::FilePath GetStartupTraceFileName(
      const base::CommandLine& command_line) const;
  void InitStartupTracingForDuration(const base::CommandLine& command_line);
  void EndStartupTracing();

  void CreateAudioManager();
  bool UsingInProcessGpu() const;

  // Quick reference for initialization order:
  // Constructor
  // Init()
  // EarlyInitialization()
  // InitializeToolkit()
  // PreMainMessageLoopStart()
  // MainMessageLoopStart()
  //   InitializeMainThread()
  // PostMainMessageLoopStart()
  //   InitStartupTracingForDuration()
  // CreateStartupTasks()
  //   PreCreateThreads()
  //   CreateThreads()
  //   BrowserThreadsStarted()
  //   PreMainMessageLoopRun()

  // Members initialized on construction ---------------------------------------
  const MainFunctionParams& parameters_;
  const base::CommandLine& parsed_command_line_;
  int result_code_;
  bool created_threads_;  // True if the non-UI threads were created.
  bool is_tracing_startup_for_duration_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  // Members initialized in |PostMainMessageLoopStart()| -----------------------
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;
  std::unique_ptr<base::HighResolutionTimerManager> hi_res_timer_manager_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Per-process listener for online state changes.
  std::unique_ptr<BrowserOnlineStateObserver> online_state_observer_;

  std::unique_ptr<base::trace_event::TraceEventSystemStatsMonitor>
      system_stats_monitor_;

#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif

#if defined(OS_WIN)
  std::unique_ptr<ScreenOrientationDelegate> screen_orientation_delegate_;
#endif

#if defined(OS_ANDROID)
  // Android implementation of ScreenOrientationDelegate
  std::unique_ptr<ScreenOrientationDelegate> screen_orientation_delegate_;
#endif

  std::unique_ptr<MemoryObserver> memory_observer_;

  // Members initialized in |InitStartupTracingForDuration()| ------------------
  base::FilePath startup_trace_file_;

  // This timer initiates trace file saving.
  base::OneShotTimer startup_trace_timer_;

  // Members initialized in |Init()| -------------------------------------------
  // Destroy |parts_| before |main_message_loop_| (required) and before other
  // classes constructed in content (but after |main_thread_|).
  std::unique_ptr<BrowserMainParts> parts_;

  // Members initialized in |InitializeMainThread()| ---------------------------
  // This must get destroyed before other threads that are created in |parts_|.
  std::unique_ptr<BrowserThreadImpl> main_thread_;

  // Members initialized in |CreateStartupTasks()| -----------------------------
  std::unique_ptr<StartupTaskRunner> startup_task_runner_;

  // Members initialized in |PreCreateThreads()| -------------------------------
  // Torn down in ShutdownThreadsAndCleanUp.
  std::unique_ptr<base::MemoryPressureMonitor> memory_pressure_monitor_;

  // Members initialized in |CreateThreads()| ----------------------------------
  std::unique_ptr<BrowserProcessSubThread> db_thread_;
  std::unique_ptr<BrowserProcessSubThread> file_user_blocking_thread_;
  std::unique_ptr<BrowserProcessSubThread> file_thread_;
  std::unique_ptr<BrowserProcessSubThread> process_launcher_thread_;
  std::unique_ptr<BrowserProcessSubThread> cache_thread_;
  std::unique_ptr<BrowserProcessSubThread> io_thread_;

  // Members initialized in |BrowserThreadsStarted()| --------------------------
  std::unique_ptr<base::Thread> indexed_db_thread_;
  std::unique_ptr<MojoShellContext> mojo_shell_context_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> mojo_ipc_support_;

  // |user_input_monitor_| has to outlive |audio_manager_|, so declared first.
  std::unique_ptr<media::UserInputMonitor> user_input_monitor_;
  // AudioThread needs to outlive |audio_manager_|.
  std::unique_ptr<base::Thread> audio_thread_;
  media::ScopedAudioManagerPtr audio_manager_;

  std::unique_ptr<media::midi::MidiManager> midi_manager_;

#if defined(OS_WIN)
  std::unique_ptr<media::SystemMessageWindowWin> system_message_window_;
#elif defined(OS_LINUX) && defined(USE_UDEV)
  std::unique_ptr<media::DeviceMonitorLinux> device_monitor_linux_;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  std::unique_ptr<media::DeviceMonitorMac> device_monitor_mac_;
#endif
#if defined(USE_OZONE)
  std::unique_ptr<ui::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif

  std::unique_ptr<LoaderDelegateImpl> loader_delegate_;
  std::unique_ptr<ResourceDispatcherHostImpl> resource_dispatcher_host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<SpeechRecognitionManagerImpl> speech_recognition_manager_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;

  // DO NOT add members here. Add them to the right categories above.

  DISALLOW_COPY_AND_ASSIGN(BrowserMainLoop);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
