// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/pending_task.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "content/child/child_process.h"
#include "content/child/content_child_helpers.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#include "third_party/skia/include/core/SkGraphics.h"
#endif  // OS_ANDROID

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>
#include <signal.h>
#include <unistd.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_pump_mac.h"
#include "third_party/WebKit/public/web/WebView.h"
#endif  // OS_MACOSX

#if defined(ENABLE_WEBRTC)
#include "third_party/libjingle/overrides/init_webrtc.h"
#endif

namespace content {
namespace {
// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kWaitForDebugger))
    base::debug::WaitForDebugger(60, true);

  if (command_line.HasSwitch(switches::kRendererStartupDialog))
    ChildProcess::WaitForDebugger("Renderer");

  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kRendererAssertTest)) {
    DCHECK(false);
  }
}

// This is a simplified version of the browser Jankometer, which measures
// the processing time of tasks on the render thread.
class RendererMessageLoopObserver : public base::MessageLoop::TaskObserver {
 public:
  RendererMessageLoopObserver()
      : process_times_(base::Histogram::FactoryGet(
            "Chrome.ProcMsgL RenderThread",
            1, 3600000, 50, base::Histogram::kUmaTargetedHistogramFlag)) {}
  virtual ~RendererMessageLoopObserver() {}

  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    begin_process_message_ = base::TimeTicks::Now();
  }

  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    if (!begin_process_message_.is_null())
      process_times_->AddTime(base::TimeTicks::Now() - begin_process_message_);
  }

 private:
  base::TimeTicks begin_process_message_;
  base::HistogramBase* const process_times_;
  DISALLOW_COPY_AND_ASSIGN(RendererMessageLoopObserver);
};

// For measuring memory usage after each task. Behind a command line flag.
class MemoryObserver : public base::MessageLoop::TaskObserver {
 public:
  MemoryObserver() {}
  virtual ~MemoryObserver() {}

  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
  }

  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    HISTOGRAM_MEMORY_KB("Memory.RendererUsed", GetMemoryUsageKB());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryObserver);
};

}  // namespace

// mainline routine for running as the Renderer process
int RendererMain(const MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN_ETW("RendererMain", 0, "");
  base::debug::TraceLog::GetInstance()->SetProcessName("Renderer");
  base::debug::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventRendererProcessSortIndex);

  const CommandLine& parsed_command_line = parameters.command_line;

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool;
#endif  // OS_MACOSX

#if defined(OS_CHROMEOS)
  // As Zygote process starts up earlier than browser process gets its own
  // locale (at login time for Chrome OS), we have to set the ICU default
  // locale for renderer process here.
  // ICU locale will be used for fallback font selection etc.
  if (parsed_command_line.HasSwitch(switches::kLang)) {
    const std::string locale =
        parsed_command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);
  }
#endif

#if defined(OS_ANDROID)
  const int kMB = 1024 * 1024;
  size_t font_cache_limit =
      base::android::SysUtils::IsLowEndDevice() ? kMB : 8 * kMB;
  SkGraphics::SetFontCacheLimit(font_cache_limit);
#endif

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(parsed_command_line);

  RendererMainPlatformDelegate platform(parameters);


  base::StatsCounterTimer stats_counter_timer("Content.RendererInit");
  base::StatsScope<base::StatsCounterTimer> startup_timer(stats_counter_timer);

  RendererMessageLoopObserver task_observer;
#if defined(OS_MACOSX)
  // As long as scrollbars on Mac are painted with Cocoa, the message pump
  // needs to be backed by a Foundation-level loop to process NSTimers. See
  // http://crbug.com/306348#c24 for details.
  scoped_ptr<base::MessagePump> pump(new base::MessagePumpNSRunLoop());
  base::MessageLoop main_message_loop(pump.Pass());
#elif defined(OS_WIN) && defined(TOOLKIT_QT)
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_DEFAULT);
#else
  // The main message loop of the renderer services doesn't have IO or UI tasks.
  base::MessageLoop main_message_loop;
#endif
  main_message_loop.AddTaskObserver(&task_observer);

  scoped_ptr<MemoryObserver> memory_observer;
  if (parsed_command_line.HasSwitch(switches::kMemoryMetrics)) {
    memory_observer.reset(new MemoryObserver());
    main_message_loop.AddTaskObserver(memory_observer.get());
  }

  base::PlatformThread::SetName("CrRendererMain");

  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);

  // Initialize histogram statistics gathering system.
  base::StatisticsRecorder::Initialize();

  // Initialize statistical testing infrastructure.  We set the entropy provider
  // to NULL to disallow the renderer process from creating its own one-time
  // randomized trials; they should be created in the browser process.
  base::FieldTrialList field_trial_list(NULL);
  // Ensure any field trials in browser are reflected into renderer.
  if (parsed_command_line.HasSwitch(switches::kForceFieldTrials)) {
    // Field trials are created in an "activated" state to ensure they get
    // reported in crash reports.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        parsed_command_line.GetSwitchValueASCII(switches::kForceFieldTrials),
        base::FieldTrialList::ACTIVATE_TRIALS,
        std::set<std::string>());
    DCHECK(result);
  }

  // PlatformInitialize uses FieldTrials, so this must happen later.
  platform.PlatformInitialize();

#if defined(ENABLE_PLUGINS)
  // Load pepper plugins before engaging the sandbox.
  PepperPluginRegistry::GetInstance();
#endif
#if defined(ENABLE_WEBRTC)
  // Initialize WebRTC before engaging the sandbox.
  // NOTE: On linux, this call could already have been made from
  // zygote_main_linux.cc.  However, calling multiple times from the same thread
  // is OK.
  InitializeWebRtcModule();
#endif

  {
#if defined(OS_WIN) || defined(OS_MACOSX)
    // TODO(markus): Check if it is OK to unconditionally move this
    // instruction down.
    RenderProcessImpl render_process;
    new RenderThreadImpl();
#endif
    bool run_loop = true;
    if (!no_sandbox) {
      run_loop = platform.EnableSandbox();
    } else {
#ifndef NDEBUG
      // For convenience, we print the stack traces for crashes.  When sandbox
      // is enabled, the in-process stack dumping is enabled as part of the
      // EnableSandbox() call.
      base::debug::EnableInProcessStackDumping();
#endif
    }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    RenderProcessImpl render_process;
    new RenderThreadImpl();
#endif

    base::HighResolutionTimerManager hi_res_timer_manager;

    startup_timer.Stop();  // End of Startup Time Measurement.

    if (run_loop) {
#if defined(OS_MACOSX)
      if (pool)
        pool->Recycle();
#endif
      TRACE_EVENT_BEGIN_ETW("RendererMain.START_MSG_LOOP", 0, 0);
      base::MessageLoop::current()->Run();
      TRACE_EVENT_END_ETW("RendererMain.START_MSG_LOOP", 0, 0);
    }
  }
  platform.PlatformUninitialize();
  TRACE_EVENT_END_ETW("RendererMain", 0, "");
  return 0;
}

}  // namespace content
