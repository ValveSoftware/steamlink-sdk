// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/gpu_host_messages.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#endif

#if defined(OS_ANDROID)
#include "base/trace_event/memory_dump_manager.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "media/gpu/dxva_video_decode_accelerator_win.h"
#include "media/gpu/media_foundation_video_encode_accelerator_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"     // nogncheck
#include "ui/gfx/x/x11_switches.h"  // nogncheck
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#include "content/common/sandbox_mac.h"
#endif

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_wrapper.h"
#endif

#if defined(SANITIZER_COVERAGE)
#include <sanitizer/common_interface_defs.h>
#include <sanitizer/coverage_interface.h>
#endif

namespace content {

namespace {

#if defined(OS_LINUX)
bool StartSandboxLinux(gpu::GpuWatchdogThread*);
#elif defined(OS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo*);
#endif

base::LazyInstance<GpuChildThread::DeferredMessages> deferred_messages =
    LAZY_INSTANCE_INITIALIZER;

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);
  deferred_messages.Get().push(
      new GpuHostMsg_OnLogMessage(severity, header, message));
  return false;
}

class ContentSandboxHelper : public gpu::GpuSandboxHelper {
 public:
  ContentSandboxHelper() {}
  ~ContentSandboxHelper() override {}

#if defined(OS_WIN)
  void set_sandbox_info(const sandbox::SandboxInterfaceInfo* info) {
    sandbox_info_ = info;
  }
#endif

 private:
  // SandboxHelper:
  void PreSandboxStartup() override {
    // Warm up resources that don't need access to GPUInfo.
    {
      TRACE_EVENT0("gpu", "Warm up rand");
      // Warm up the random subsystem, which needs to be done pre-sandbox on all
      // platforms.
      (void)base::RandUint64();
    }

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
    media::VaapiWrapper::PreSandboxInitialization();
#endif
#if defined(OS_WIN)
    media::DXVAVideoDecodeAccelerator::PreSandboxInitialization();
    media::MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization();
#endif
  }

  bool EnsureSandboxInitialized(
      gpu::GpuWatchdogThread* watchdog_thread) override {
#if defined(OS_LINUX)
    return StartSandboxLinux(watchdog_thread);
#elif defined(OS_WIN)
    return StartSandboxWindows(sandbox_info_);
#elif defined(OS_MACOSX)
    return Sandbox::SandboxIsCurrentlyActive();
#else
    return false;
#endif
  }

#if defined(OS_WIN)
  const sandbox::SandboxInterfaceInfo* sandbox_info_ = nullptr;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentSandboxHelper);
};

}  // namespace

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  TRACE_EVENT0("gpu", "GpuMain");
  base::trace_event::TraceLog::GetInstance()->SetProcessName("GPU Process");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventGpuProcessSortIndex);

  const base::CommandLine& command_line = parameters.command_line;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger("Gpu");
  }

  base::Time start_time = base::Time::Now();

#if defined(OS_WIN)
  // Prevent Windows from displaying a modal dialog on failures like not being
  // able to load a DLL.
  SetErrorMode(
      SEM_FAILCRITICALERRORS |
      SEM_NOGPFAULTERRORBOX |
      SEM_NOOPENFILEERRORBOX);
#elif defined(USE_X11)
  ui::SetDefaultX11ErrorHandlers();

#endif

  logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

#if defined(OS_WIN)
  // OK to use default non-UI message loop because all GPU windows run on
  // dedicated thread.
#if defined(TOOLKIT_QT)
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_DEFAULT);
#else
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);
#endif
#elif defined(USE_X11)
  // We need a UI loop so that we can grab the Expose events. See GLSurfaceGLX
  // and https://crbug.com/326995.
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);
  std::unique_ptr<ui::PlatformEventSource> event_source =
      ui::PlatformEventSource::CreateDefault();
#elif defined(USE_OZONE) && defined(OZONE_X11)
  // If we might be running Ozone X11 we need a UI loop to grab Expose events.
  // See GLSurfaceGLX and https://crbug.com/326995.
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);
#elif defined(USE_OZONE)
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_DEFAULT);
#elif defined(OS_LINUX)
#error "Unsupported Linux platform."
#elif defined(OS_MACOSX)
  // This is necessary for CoreAnimation layers hosted in the GPU process to be
  // drawn. See http://crbug.com/312462.
  std::unique_ptr<base::MessagePump> pump(new base::MessagePumpCFRunLoop());
  base::MessageLoop main_message_loop(std::move(pump));
#else
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_IO);
#endif

  base::PlatformThread::SetName("CrGpuMain");

  // Initializes StatisticsRecorder which tracks UMA histograms.
  base::StatisticsRecorder::Initialize();

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Set thread priority before sandbox initialization.
  base::PlatformThread::SetCurrentThreadPriority(base::ThreadPriority::DISPLAY);
#endif

  gpu::GpuInit gpu_init;
  ContentSandboxHelper sandbox_helper;
#if defined(OS_WIN)
  sandbox_helper.set_sandbox_info(parameters.sandbox_info);
#endif

  gpu_init.set_sandbox_helper(&sandbox_helper);

  // Gpu initialization may fail for various reasons, in which case we will need
  // to tear down this process. However, we can not do so safely until the IPC
  // channel is set up, because the detection of early return of a child process
  // is implemented using an IPC channel error. If the IPC channel is not fully
  // set up between the browser and GPU process, and the GPU process crashes or
  // exits early, the browser process will never detect it.  For this reason we
  // defer tearing down the GPU process until receiving the GpuMsg_Initialize
  // message from the browser.
  const bool init_success = gpu_init.InitializeAndStartSandbox(command_line);
  const bool dead_on_arrival = !init_success;

  logging::SetLogMessageHandler(NULL);
  GetContentClient()->SetGpuInfo(gpu_init.gpu_info());

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory;
  if (init_success &&
      gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER)
    gpu_memory_buffer_factory = gpu::GpuMemoryBufferFactory::CreateNativeType();

  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  io_thread_priority = base::ThreadPriority::DISPLAY;
#endif

  GpuProcess gpu_process(io_thread_priority);
  GpuChildThread* child_thread = new GpuChildThread(
      gpu_init.TakeWatchdogThread(), dead_on_arrival, gpu_init.gpu_info(),
      deferred_messages.Get(), gpu_memory_buffer_factory.get());
  while (!deferred_messages.Get().empty())
    deferred_messages.Get().pop();

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  if (child_thread->watchdog_thread())
    child_thread->watchdog_thread()->AddPowerObserver();

#if defined(OS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);
#endif

  {
    TRACE_EVENT0("gpu", "Run Message Loop");
    base::RunLoop().Run();
  }

  return dead_on_arrival ? 2 : 0;
}

namespace {

#if defined(OS_LINUX)
bool StartSandboxLinux(gpu::GpuWatchdogThread* watchdog_thread) {
  TRACE_EVENT0("gpu,startup", "Initialize sandbox");

  bool res = false;

  if (watchdog_thread) {
    // LinuxSandbox needs to be able to ensure that the thread
    // has really been stopped.
    LinuxSandbox::StopThread(watchdog_thread);
  }

#if defined(SANITIZER_COVERAGE)
  const std::string sancov_file_name =
      "gpu." + base::Uint64ToString(base::RandUint64());
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  linux_sandbox->sanitizer_args()->coverage_sandboxed = 1;
  linux_sandbox->sanitizer_args()->coverage_fd =
      __sanitizer_maybe_open_cov_file(sancov_file_name.c_str());
  linux_sandbox->sanitizer_args()->coverage_max_block_size = 0;
#endif

  // LinuxSandbox::InitializeSandbox() must always be called
  // with only one thread.
  res = LinuxSandbox::InitializeSandbox();
  if (watchdog_thread) {
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    watchdog_thread->StartWithOptions(options);
  }

  return res;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo* sandbox_info) {
  TRACE_EVENT0("gpu,startup", "Lower token");

  // For Windows, if the target_services interface is not zero, the process
  // is sandboxed and we must call LowerToken() before rendering untrusted
  // content.
  sandbox::TargetServices* target_services = sandbox_info->target_services;
  if (target_services) {
    target_services->LowerToken();
    return true;
  }

  return false;
}
#endif  // defined(OS_WIN)

}  // namespace.

}  // namespace content
