// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_info_collector.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_OZONE)
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/public/gpu_platform_support.h"
#endif

namespace content {
namespace {

static base::LazyInstance<scoped_refptr<ThreadSafeSender> >
    g_thread_safe_sender = LAZY_INSTANCE_INITIALIZER;

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);

  g_thread_safe_sender.Get()->Send(new GpuHostMsg_OnLogMessage(
      severity, header, message));

  return false;
}

}  // namespace

GpuChildThread* GpuChildThread::instance_ = 0;

GpuChildThread::GpuChildThread(GpuWatchdogThread* watchdog_thread,
                               bool dead_on_arrival,
                               const gpu::GPUInfo& gpu_info,
                               const DeferredMessages& deferred_messages)
    : dead_on_arrival_(dead_on_arrival),
      gpu_info_(gpu_info),
      deferred_messages_(deferred_messages),
      in_browser_process_(false) {
  watchdog_thread_ = watchdog_thread;
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  g_thread_safe_sender.Get() = thread_safe_sender();

  instance_ = this;
}

GpuChildThread::GpuChildThread(const std::string& channel_id)
    : ChildThread(channel_id),
      dead_on_arrival_(false),
      in_browser_process_(true) {
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  DCHECK(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU));
#if !defined(OS_ANDROID)
  // For single process and in-process GPU mode, we need to load and
  // initialize the GL implementation and locate the GL entry points here.
  // On Android, GLSurface::InitializeOneOff() is called from BrowserMainLoop
  // before getting here. crbug.com/326295
  if (!gfx::GLSurface::InitializeOneOff())
    VLOG(1) << "gfx::GLSurface::InitializeOneOff failed";
#endif
  g_thread_safe_sender.Get() = thread_safe_sender();

  instance_ = this;
}

GpuChildThread::~GpuChildThread() {
}

void GpuChildThread::Shutdown() {
  ChildThread::Shutdown();
  logging::SetLogMessageHandler(NULL);
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThread::Send(msg);
}

bool GpuChildThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChildThread, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
    IPC_MESSAGE_HANDLER(GpuMsg_GetVideoMemoryUsageStats,
                        OnGetVideoMemoryUsageStats)
    IPC_MESSAGE_HANDLER(GpuMsg_Clean, OnClean)
    IPC_MESSAGE_HANDLER(GpuMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang, OnHang)
    IPC_MESSAGE_HANDLER(GpuMsg_DisableWatchdog, OnDisableWatchdog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

#if defined(USE_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetGpuPlatformSupport()
          ->OnMessageReceived(msg))
    return true;
#endif

  return gpu_channel_manager_.get() &&
      gpu_channel_manager_->OnMessageReceived(msg);
}

void GpuChildThread::OnInitialize() {
  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - process_start_time_;
  Send(new GpuHostMsg_Initialized(!dead_on_arrival_, gpu_info_));
  while (!deferred_messages_.empty()) {
    Send(deferred_messages_.front());
    deferred_messages_.pop();
  }

  if (dead_on_arrival_) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    base::MessageLoop::current()->Quit();
    return;
  }

#if defined(OS_ANDROID)
  base::PlatformThread::SetThreadPriority(
      base::PlatformThread::CurrentHandle(),
      base::kThreadPriority_Display);
#endif

  // We don't need to pipe log messages if we are running the GPU thread in
  // the browser process.
  if (!in_browser_process_)
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(
      new GpuChannelManager(GetRouter(),
                            watchdog_thread_.get(),
                            ChildProcess::current()->io_message_loop_proxy(),
                            ChildProcess::current()->GetShutDownEvent()));

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupport()
      ->OnChannelEstablished(this);
#endif
}

void GpuChildThread::StopWatchdog() {
  if (watchdog_thread_.get()) {
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnCollectGraphicsInfo() {
#if defined(OS_WIN)
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(switches::kDisableGpuSandbox) ||
         in_browser_process_);
#endif  // OS_WIN

  gpu::CollectInfoResult result =
      gpu::CollectContextGraphicsInfo(&gpu_info_);
  switch (result) {
    case gpu::kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      // TODO(piman): can we signal overall failure?
      break;
    case gpu::kCollectInfoNonFatalFailure:
      VLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoSuccess:
      break;
  }
  GetContentClient()->SetGpuInfo(gpu_info_);

#if defined(OS_WIN)
  // This is slow, but it's the only thing the unsandboxed GPU process does,
  // and GpuDataManager prevents us from sending multiple collecting requests,
  // so it's OK to be blocking.
  gpu::GetDxDiagnostics(&gpu_info_.dx_diagnostics);
  gpu_info_.finalized = true;
#endif  // OS_WIN

  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));

#if defined(OS_WIN)
  if (!in_browser_process_) {
    // The unsandboxed GPU process fulfilled its duty.  Rest in peace.
    base::MessageLoop::current()->Quit();
  }
#endif  // OS_WIN
}

void GpuChildThread::OnGetVideoMemoryUsageStats() {
  GPUVideoMemoryUsageStats video_memory_usage_stats;
  if (gpu_channel_manager_)
    gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
        &video_memory_usage_stats);
  Send(new GpuHostMsg_VideoMemoryUsageStats(video_memory_usage_stats));
}

void GpuChildThread::OnClean() {
  VLOG(1) << "GPU: Removing all contexts";
  if (gpu_channel_manager_)
    gpu_channel_manager_->LoseAllContexts();
}

void GpuChildThread::OnCrash() {
  VLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuChildThread::OnHang() {
  VLOG(1) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

void GpuChildThread::OnDisableWatchdog() {
  VLOG(1) << "GPU: Disabling watchdog thread";
  if (watchdog_thread_.get()) {
    // Disarm the watchdog before shutting down the message loop. This prevents
    // the future posting of tasks to the message loop.
    if (watchdog_thread_->message_loop())
      watchdog_thread_->PostAcknowledge();
    // Prevent rearming.
    watchdog_thread_->Stop();
  }
}

}  // namespace content

