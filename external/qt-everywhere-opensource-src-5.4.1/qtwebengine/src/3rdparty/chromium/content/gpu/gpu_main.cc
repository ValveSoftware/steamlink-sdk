// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#if defined(OS_WIN)
#include <dwmapi.h>
#include <windows.h>
#endif

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/gpu/gpu_config.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "base/win/scoped_com_initializer.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#endif

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#endif

const int kGpuTimeout = 10000;

namespace content {

namespace {

void GetGpuInfoFromCommandLine(gpu::GPUInfo& gpu_info,
                               const CommandLine& command_line);
bool WarmUpSandbox(const CommandLine& command_line);

#if !defined(OS_MACOSX)
bool CollectGraphicsInfo(gpu::GPUInfo& gpu_info);
#endif

#if defined(OS_LINUX)
#if !defined(OS_CHROMEOS)
bool CanAccessNvidiaDeviceFile();
#endif
bool StartSandboxLinux(const gpu::GPUInfo&, GpuWatchdogThread*, bool);
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
  deferred_messages.Get().push(new GpuHostMsg_OnLogMessage(
      severity, header, message));
  return false;
}

}  // namespace anonymous

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  TRACE_EVENT0("gpu", "GpuMain");
  base::debug::TraceLog::GetInstance()->SetProcessName("GPU Process");
  base::debug::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventGpuProcessSortIndex);

  const CommandLine& command_line = parameters.command_line;
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

  if (command_line.HasSwitch(switches::kSupportsDualGpus)) {
    std::string types = command_line.GetSwitchValueASCII(
        switches::kGpuDriverBugWorkarounds);
    std::set<int> workarounds;
    gpu::StringToFeatureSet(types, &workarounds);
    if (workarounds.count(gpu::FORCE_DISCRETE_GPU) == 1)
      ui::GpuSwitchingManager::GetInstance()->ForceUseOfDiscreteGpu();
    else if (workarounds.count(gpu::FORCE_INTEGRATED_GPU) == 1)
      ui::GpuSwitchingManager::GetInstance()->ForceUseOfIntegratedGpu();
  }

  // Initialization of the OpenGL bindings may fail, in which case we
  // will need to tear down this process. However, we can not do so
  // safely until the IPC channel is set up, because the detection of
  // early return of a child process is implemented using an IPC
  // channel error. If the IPC channel is not fully set up between the
  // browser and GPU process, and the GPU process crashes or exits
  // early, the browser process will never detect it.  For this reason
  // we defer tearing down the GPU process until receiving the
  // GpuMsg_Initialize message from the browser.
  bool dead_on_arrival = false;

#if defined(OS_WIN)
  base::MessageLoop::Type message_loop_type = base::MessageLoop::TYPE_IO;
  // Unless we're running on desktop GL, we don't need a UI message
  // loop, so avoid its use to work around apparent problems with some
  // third-party software.
  if (command_line.HasSwitch(switches::kUseGL) &&
      command_line.GetSwitchValueASCII(switches::kUseGL) ==
          gfx::kGLImplementationDesktopName) {
    message_loop_type = base::MessageLoop::TYPE_UI;
  }
  base::MessageLoop main_message_loop(message_loop_type);
#elif defined(OS_LINUX) && defined(USE_X11)
  // We need a UI loop so that we can grab the Expose events. See GLSurfaceGLX
  // and https://crbug.com/326995.
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);
  scoped_ptr<ui::PlatformEventSource> event_source =
      ui::PlatformEventSource::CreateDefault();
#elif defined(OS_LINUX)
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_DEFAULT);
#elif defined(OS_MACOSX)
  // This is necessary for CoreAnimation layers hosted in the GPU process to be
  // drawn. See http://crbug.com/312462.
  scoped_ptr<base::MessagePump> pump(new base::MessagePumpCFRunLoop());
  base::MessageLoop main_message_loop(pump.Pass());
#else
  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_IO);
#endif

  base::PlatformThread::SetName("CrGpuMain");

  // In addition to disabling the watchdog if the command line switch is
  // present, disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      !command_line.HasSwitch(switches::kDisableGpuWatchdog) &&
      !RunningOnValgrind();

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  bool delayed_watchdog_enable = false;

#if defined(OS_CHROMEOS)
  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  delayed_watchdog_enable = true;
#endif

  scoped_refptr<GpuWatchdogThread> watchdog_thread;

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread = new GpuWatchdogThread(kGpuTimeout);
    watchdog_thread->Start();
  }

  gpu::GPUInfo gpu_info;
  // Get vendor_id, device_id, driver_version from browser process through
  // commandline switches.
  GetGpuInfoFromCommandLine(gpu_info, command_line);

  base::TimeDelta collect_context_time;
  base::TimeDelta initialize_one_off_time;

  // Warm up resources that don't need access to GPUInfo.
  if (WarmUpSandbox(command_line)) {
#if defined(OS_LINUX)
    bool initialized_sandbox = false;
    bool initialized_gl_context = false;
    bool should_initialize_gl_context = false;
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
    // On Chrome OS ARM Mali, GPU driver userspace creates threads when
    // initializing a GL context, so start the sandbox early.
    if (!command_line.HasSwitch(
             switches::kGpuSandboxStartAfterInitialization)) {
      gpu_info.sandboxed = StartSandboxLinux(gpu_info, watchdog_thread.get(),
                                             should_initialize_gl_context);
      initialized_sandbox = true;
    }
#endif
#endif  // defined(OS_LINUX)

    base::TimeTicks before_initialize_one_off = base::TimeTicks::Now();

    // Determine if we need to initialize GL here or it has already been done.
    bool gl_already_initialized = false;
#if defined(OS_MACOSX)
    if (!command_line.HasSwitch(switches::kNoSandbox)) {
      // On Mac, if the sandbox is enabled, then GLSurface::InitializeOneOff()
      // is called from the sandbox warmup code before getting here.
      gl_already_initialized = true;
    }
#endif
    if (command_line.HasSwitch(switches::kInProcessGPU)) {
      // With in-process GPU, GLSurface::InitializeOneOff() is called from
      // GpuChildThread before getting here.
      gl_already_initialized = true;
    }

    // Load and initialize the GL implementation and locate the GL entry points.
    bool gl_initialized =
        gl_already_initialized
            ? gfx::GetGLImplementation() != gfx::kGLImplementationNone
            : gfx::GLSurface::InitializeOneOff();
    if (gl_initialized) {
      // We need to collect GL strings (VENDOR, RENDERER) for blacklisting
      // purposes. However, on Mac we don't actually use them. As documented in
      // crbug.com/222934, due to some driver issues, glGetString could take
      // multiple seconds to finish, which in turn cause the GPU process to
      // crash.
      // By skipping the following code on Mac, we don't really lose anything,
      // because the basic GPU information is passed down from browser process
      // and we already registered them through SetGpuInfo() above.
      base::TimeTicks before_collect_context_graphics_info =
          base::TimeTicks::Now();
#if !defined(OS_MACOSX)
      if (!CollectGraphicsInfo(gpu_info))
        dead_on_arrival = true;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
      // Recompute gpu driver bug workarounds - this is specifically useful
      // on systems where vendor_id/device_id aren't available.
      if (!command_line.HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
        gpu::ApplyGpuDriverBugWorkarounds(
            gpu_info, const_cast<CommandLine*>(&command_line));
      }
#endif

#if defined(OS_LINUX)
      initialized_gl_context = true;
#if !defined(OS_CHROMEOS)
      if (gpu_info.gpu.vendor_id == 0x10de &&  // NVIDIA
          gpu_info.driver_vendor == "NVIDIA" &&
          !CanAccessNvidiaDeviceFile())
        dead_on_arrival = true;
#endif  // !defined(OS_CHROMEOS)
#endif  // defined(OS_LINUX)
#endif  // !defined(OS_MACOSX)
      collect_context_time =
          base::TimeTicks::Now() - before_collect_context_graphics_info;
    } else {  // gl_initialized
      VLOG(1) << "gfx::GLSurface::InitializeOneOff failed";
      dead_on_arrival = true;
    }

    initialize_one_off_time =
        base::TimeTicks::Now() - before_initialize_one_off;

    if (enable_watchdog && delayed_watchdog_enable) {
      watchdog_thread = new GpuWatchdogThread(kGpuTimeout);
      watchdog_thread->Start();
    }

    // OSMesa is expected to run very slowly, so disable the watchdog in that
    // case.
    if (enable_watchdog &&
        gfx::GetGLImplementation() == gfx::kGLImplementationOSMesaGL) {
      watchdog_thread->Stop();
      watchdog_thread = NULL;
    }

#if defined(OS_LINUX)
    should_initialize_gl_context = !initialized_gl_context &&
                                   !dead_on_arrival;

    if (!initialized_sandbox) {
      gpu_info.sandboxed = StartSandboxLinux(gpu_info, watchdog_thread.get(),
                                             should_initialize_gl_context);
    }
#elif defined(OS_WIN)
    gpu_info.sandboxed = StartSandboxWindows(parameters.sandbox_info);
#endif
  } else {
    dead_on_arrival = true;
  }

  logging::SetLogMessageHandler(NULL);

  GpuProcess gpu_process;

  // These UMA must be stored after GpuProcess is constructed as it
  // initializes StatisticsRecorder which tracks the histograms.
  UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);
  UMA_HISTOGRAM_TIMES("GPU.InitializeOneOffTime", initialize_one_off_time);

  GpuChildThread* child_thread = new GpuChildThread(watchdog_thread.get(),
                                                    dead_on_arrival,
                                                    gpu_info,
                                                    deferred_messages.Get());
  while (!deferred_messages.Get().empty())
    deferred_messages.Get().pop();

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  if (watchdog_thread)
    watchdog_thread->AddPowerObserver();

  {
    TRACE_EVENT0("gpu", "Run Message Loop");
    main_message_loop.Run();
  }

  child_thread->StopWatchdog();

  return 0;
}

namespace {

void GetGpuInfoFromCommandLine(gpu::GPUInfo& gpu_info,
                               const CommandLine& command_line) {
  DCHECK(command_line.HasSwitch(switches::kGpuVendorID) &&
         command_line.HasSwitch(switches::kGpuDeviceID) &&
         command_line.HasSwitch(switches::kGpuDriverVersion));
  bool success = base::HexStringToUInt(
      command_line.GetSwitchValueASCII(switches::kGpuVendorID),
      &gpu_info.gpu.vendor_id);
  DCHECK(success);
  success = base::HexStringToUInt(
      command_line.GetSwitchValueASCII(switches::kGpuDeviceID),
      &gpu_info.gpu.device_id);
  DCHECK(success);
  gpu_info.driver_vendor =
      command_line.GetSwitchValueASCII(switches::kGpuDriverVendor);
  gpu_info.driver_version =
      command_line.GetSwitchValueASCII(switches::kGpuDriverVersion);
  GetContentClient()->SetGpuInfo(gpu_info);
}

bool WarmUpSandbox(const CommandLine& command_line) {
  {
    TRACE_EVENT0("gpu", "Warm up rand");
    // Warm up the random subsystem, which needs to be done pre-sandbox on all
    // platforms.
    (void) base::RandUint64();
  }
  return true;
}

#if !defined(OS_MACOSX)
bool CollectGraphicsInfo(gpu::GPUInfo& gpu_info) {
  bool res = true;
  gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info);
  switch (result) {
    case gpu::kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      res = false;
      break;
    case gpu::kCollectInfoNonFatalFailure:
      VLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoSuccess:
      break;
  }
  GetContentClient()->SetGpuInfo(gpu_info);
  return res;
}
#endif

#if defined(OS_LINUX)
#if !defined(OS_CHROMEOS)
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ThreadRestrictions::AssertIOAllowed();
  if (access("/dev/nvidiactl", R_OK) != 0) {
    VLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif

void CreateDummyGlContext() {
  scoped_refptr<gfx::GLSurface> surface(
      gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size()));
  if (!surface.get()) {
    VLOG(1) << "gfx::GLSurface::CreateOffscreenGLSurface failed";
    return;
  }

  // On Linux, this is needed to make sure /dev/nvidiactl has
  // been opened and its descriptor cached.
  scoped_refptr<gfx::GLContext> context(gfx::GLContext::CreateGLContext(
      NULL, surface.get(), gfx::PreferDiscreteGpu));
  if (!context.get()) {
    VLOG(1) << "gfx::GLContext::CreateGLContext failed";
    return;
  }

  // Similarly, this is needed for /dev/nvidia0.
  if (context->MakeCurrent(surface.get())) {
    context->ReleaseCurrent(surface.get());
  } else {
    VLOG(1)  << "gfx::GLContext::MakeCurrent failed";
  }
}

void WarmUpSandboxNvidia(const gpu::GPUInfo& gpu_info,
                         bool should_initialize_gl_context) {
  // We special case Optimus since the vendor_id we see may not be Nvidia.
  bool uses_nvidia_driver = (gpu_info.gpu.vendor_id == 0x10de &&  // NVIDIA.
                             gpu_info.driver_vendor == "NVIDIA") ||
                            gpu_info.optimus;
  if (uses_nvidia_driver && should_initialize_gl_context) {
    // We need this on Nvidia to pre-open /dev/nvidiactl and /dev/nvidia0.
    CreateDummyGlContext();
  }
}

bool StartSandboxLinux(const gpu::GPUInfo& gpu_info,
                       GpuWatchdogThread* watchdog_thread,
                       bool should_initialize_gl_context) {
  TRACE_EVENT0("gpu", "Initialize sandbox");

  bool res = false;

  WarmUpSandboxNvidia(gpu_info, should_initialize_gl_context);

  if (watchdog_thread) {
    // LinuxSandbox needs to be able to ensure that the thread
    // has really been stopped.
    LinuxSandbox::StopThread(watchdog_thread);
  }

#if defined(ADDRESS_SANITIZER)
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
    watchdog_thread->Start();
  }

  return res;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo* sandbox_info) {
  TRACE_EVENT0("gpu", "Lower token");

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
