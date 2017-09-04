// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

namespace {

void GetGpuInfoFromCommandLine(gpu::GPUInfo& gpu_info,
                               const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kGpuVendorID) ||
      !command_line.HasSwitch(switches::kGpuDeviceID) ||
      !command_line.HasSwitch(switches::kGpuDriverVersion))
    return;
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
  gpu_info.driver_date =
      command_line.GetSwitchValueASCII(switches::kGpuDriverDate);
  gpu::ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  // Set active gpu device.
  if (command_line.HasSwitch(switches::kGpuActiveVendorID) &&
      command_line.HasSwitch(switches::kGpuActiveDeviceID)) {
    uint32_t active_vendor_id = 0;
    uint32_t active_device_id = 0;
    success = base::HexStringToUInt(
        command_line.GetSwitchValueASCII(switches::kGpuActiveVendorID),
        &active_vendor_id);
    DCHECK(success);
    success = base::HexStringToUInt(
        command_line.GetSwitchValueASCII(switches::kGpuActiveDeviceID),
        &active_device_id);
    DCHECK(success);
    if (gpu_info.gpu.vendor_id == active_vendor_id &&
        gpu_info.gpu.device_id == active_device_id) {
      gpu_info.gpu.active = true;
    } else {
      for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
        if (gpu_info.secondary_gpus[i].vendor_id == active_vendor_id &&
            gpu_info.secondary_gpus[i].device_id == active_device_id) {
          gpu_info.secondary_gpus[i].active = true;
          break;
        }
      }
    }
  }
}

#if !defined(OS_MACOSX)
void CollectGraphicsInfo(gpu::GPUInfo& gpu_info) {
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");

  gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info);
  switch (result) {
    case gpu::kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      break;
    case gpu::kCollectInfoNonFatalFailure:
      DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoNone:
      NOTREACHED();
      break;
    case gpu::kCollectInfoSuccess:
      break;
  }
}
#endif  // defined(OS_MACOSX)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ThreadRestrictions::AssertIOAllowed();
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

}  // namespace

GpuInit::GpuInit() {}

GpuInit::~GpuInit() {}

bool GpuInit::InitializeAndStartSandbox(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSupportsDualGpus)) {
    std::set<int> workarounds;
    gpu::GpuDriverBugList::AppendWorkaroundsFromCommandLine(&workarounds,
                                                            command_line);
    gpu::InitializeDualGpusIfSupported(workarounds);
  }

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

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable)
    watchdog_thread_ = gpu::GpuWatchdogThread::Create();

  // Get vendor_id, device_id, driver_version from browser process through
  // commandline switches.
  GetGpuInfoFromCommandLine(gpu_info_, command_line);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (gpu_info_.gpu.vendor_id == 0x10de &&  // NVIDIA
      gpu_info_.driver_vendor == "NVIDIA" && !CanAccessNvidiaDeviceFile())
    return false;
#endif
  gpu_info_.in_process_gpu = false;

  sandbox_helper_->PreSandboxStartup();

#if defined(OS_LINUX)
  // On Chrome OS ARM Mali, GPU driver userspace creates threads when
  // initializing a GL context, so start the sandbox early.
  if (command_line.HasSwitch(switches::kGpuSandboxStartEarly))
    gpu_info_.sandboxed =
        sandbox_helper_->EnsureSandboxInitialized(watchdog_thread_.get());
#endif  // defined(OS_LINUX)

  base::TimeTicks before_initialize_one_off = base::TimeTicks::Now();

  // Load and initialize the GL implementation and locate the GL entry points if
  // needed. This initialization may have already happened if running in the
  // browser process, for example.
  bool gl_initialized = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (!gl_initialized)
    gl_initialized = gl::init::InitializeGLOneOff();

  if (!gl_initialized) {
    VLOG(1) << "gl::init::InitializeGLOneOff failed";
    return false;
  }

  // We need to collect GL strings (VENDOR, RENDERER) for blacklisting purposes.
  // However, on Mac we don't actually use them. As documented in
  // crbug.com/222934, due to some driver issues, glGetString could take
  // multiple seconds to finish, which in turn cause the GPU process to crash.
  // By skipping the following code on Mac, we don't really lose anything,
  // because the basic GPU information is passed down from the host process.
  base::TimeTicks before_collect_context_graphics_info = base::TimeTicks::Now();
#if !defined(OS_MACOSX)
  CollectGraphicsInfo(gpu_info_);
  if (gpu_info_.context_info_state == gpu::kCollectInfoFatalFailure)
    return false;

  // Recompute gpu driver bug workarounds.
  // This is necessary on systems where vendor_id/device_id aren't available
  // (Chrome OS, Android) or where workarounds may be dependent on GL_VENDOR
  // and GL_RENDERER strings which are lazily computed (Linux).
  if (!command_line.HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    // TODO: this can not affect disabled extensions, since they're already
    // initialized in the bindings. This should be moved before bindings
    // initialization. However, populating GPUInfo fully works only on Android.
    // Other platforms would need the bindings to query GL strings.
    gpu::ApplyGpuDriverBugWorkarounds(
        gpu_info_, const_cast<base::CommandLine*>(&command_line));
  }
#endif  // !defined(OS_MACOSX)

  base::TimeDelta collect_context_time =
      base::TimeTicks::Now() - before_collect_context_graphics_info;
  UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);

  base::TimeDelta initialize_one_off_time =
      base::TimeTicks::Now() - before_initialize_one_off;
  UMA_HISTOGRAM_MEDIUM_TIMES("GPU.InitializeOneOffMediumTime",
                             initialize_one_off_time);

  // OSMesa is expected to run very slowly, so disable the watchdog in that
  // case.
  if (gl::GetGLImplementation() == gl::kGLImplementationOSMesaGL) {
    if (watchdog_thread_)
      watchdog_thread_->Stop();
    watchdog_thread_ = nullptr;
  } else if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create();
  }

  if (!gpu_info_.sandboxed)
    gpu_info_.sandboxed =
        sandbox_helper_->EnsureSandboxInitialized(watchdog_thread_.get());
  return true;
}

}  // namespace gpu
