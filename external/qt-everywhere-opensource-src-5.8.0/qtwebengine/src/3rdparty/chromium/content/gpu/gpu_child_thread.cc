// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_local.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/establish_channel_params.h"
#include "content/common/gpu_host_messages.h"
#include "content/gpu/gpu_process_control_impl.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_service.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_client_android.h"
#include "media/gpu/avda_surface_tracker.h"
#endif

namespace content {
namespace {

base::LazyInstance<base::ThreadLocalPointer<GpuChildThread>> g_lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<scoped_refptr<ThreadSafeSender> >
    g_thread_safe_sender = LAZY_INSTANCE_INITIALIZER;

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);

  g_thread_safe_sender.Get()->Send(
      new GpuHostMsg_OnLogMessage(severity, header, message));

  return false;
}

// Message filter used to to handle GpuMsg_CreateGpuMemoryBuffer messages
// on the IO thread. This allows the UI thread in the browser process to remain
// fast at all times.
class GpuMemoryBufferMessageFilter : public IPC::MessageFilter {
 public:
  explicit GpuMemoryBufferMessageFilter(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
      : gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
        sender_(nullptr) {}

  // Overridden from IPC::MessageFilter:
  void OnFilterAdded(IPC::Sender* sender) override {
    DCHECK(!sender_);
    sender_ = sender;
  }
  void OnFilterRemoved() override {
    DCHECK(sender_);
    sender_ = nullptr;
  }
  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK(sender_);
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuMemoryBufferMessageFilter, message)
      IPC_MESSAGE_HANDLER(GpuMsg_CreateGpuMemoryBuffer, OnCreateGpuMemoryBuffer)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 protected:
  ~GpuMemoryBufferMessageFilter() override {}

  void OnCreateGpuMemoryBuffer(
      const GpuMsg_CreateGpuMemoryBuffer_Params& params) {
    TRACE_EVENT2("gpu", "GpuMemoryBufferMessageFilter::OnCreateGpuMemoryBuffer",
                 "id", params.id.id, "client_id", params.client_id);

    DCHECK(gpu_memory_buffer_factory_);
    sender_->Send(new GpuHostMsg_GpuMemoryBufferCreated(
        gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
            params.id, params.size, params.format, params.usage,
            params.client_id, params.surface_handle)));
  }

  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  IPC::Sender* sender_;
};

ChildThreadImpl::Options GetOptions(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory) {
  ChildThreadImpl::Options::Builder builder;

  builder.AddStartupFilter(
      new GpuMemoryBufferMessageFilter(gpu_memory_buffer_factory));

#if defined(USE_OZONE)
  IPC::MessageFilter* message_filter = ui::OzonePlatform::GetInstance()
                                           ->GetGpuPlatformSupport()
                                           ->GetMessageFilter();
  if (message_filter)
    builder.AddStartupFilter(message_filter);
#endif

  return builder.Build();
}

}  // namespace

// FIXME: try replacing instance_ with current().
GpuChildThread* GpuChildThread::instance_ = 0;

// static
GpuChildThread* GpuChildThread::current() {
  return g_lazy_tls.Pointer()->Get();
}

GpuChildThread::GpuChildThread(
    GpuWatchdogThread* watchdog_thread,
    bool dead_on_arrival,
    const gpu::GPUInfo& gpu_info,
    const DeferredMessages& deferred_messages,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : ChildThreadImpl(GetOptions(gpu_memory_buffer_factory)),
      dead_on_arrival_(dead_on_arrival),
      gpu_info_(gpu_info),
      deferred_messages_(deferred_messages),
      in_browser_process_(false),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  watchdog_thread_ = watchdog_thread;
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  g_thread_safe_sender.Get() = thread_safe_sender();
  g_lazy_tls.Pointer()->Set(this);

  instance_ = this;
}

GpuChildThread::GpuChildThread(
    const gpu::GpuPreferences& gpu_preferences,
    const InProcessChildThreadParams& params,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : ChildThreadImpl(ChildThreadImpl::Options::Builder()
                          .InBrowserProcess(params)
                          .AddStartupFilter(new GpuMemoryBufferMessageFilter(
                              gpu_memory_buffer_factory))
                          .Build()),
      gpu_preferences_(gpu_preferences),
      dead_on_arrival_(false),
      in_browser_process_(true),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kInProcessGPU));

  if (!gl::init::InitializeGLOneOff())
    VLOG(1) << "gl::init::InitializeGLOneOff failed";

  g_thread_safe_sender.Get() = thread_safe_sender();
  g_lazy_tls.Pointer()->Set(this);

  instance_ = this;
}

GpuChildThread::~GpuChildThread() {
  while (!deferred_messages_.empty()) {
    delete deferred_messages_.front();
    deferred_messages_.pop();
  }
  g_lazy_tls.Pointer()->Set(nullptr);
}

void GpuChildThread::Shutdown() {
  ChildThreadImpl::Shutdown();
  logging::SetLogMessageHandler(NULL);
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;

#if defined(OS_ANDROID)
  // When running in in-process mode, this has been set in the browser at
  // ChromeBrowserMainPartsAndroid::PreMainMessageLoopRun().
  if (!in_browser_process_)
    media::SetMediaClientAndroid(GetContentClient()->GetMediaClientAndroid());
#endif

  // Only set once per process instance.
  process_control_.reset(new GpuProcessControlImpl());

  if (GetContentClient()->gpu())  // NULL in tests.
    GetContentClient()->gpu()->Initialize(this);
}

void GpuChildThread::OnFieldTrialGroupFinalized(const std::string& trial_name,
                                                const std::string& group_name) {
  Send(new GpuHostMsg_FieldTrialActivated(trial_name));
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThreadImpl::Send(msg);
}

bool GpuChildThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChildThread, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(GpuMsg_Finalize, OnFinalize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
    IPC_MESSAGE_HANDLER(GpuMsg_GetVideoMemoryUsageStats,
                        OnGetVideoMemoryUsageStats)
    IPC_MESSAGE_HANDLER(GpuMsg_Clean, OnClean)
    IPC_MESSAGE_HANDLER(GpuMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang, OnHang)
    IPC_MESSAGE_HANDLER(GpuMsg_DisableWatchdog, OnDisableWatchdog)
    IPC_MESSAGE_HANDLER(GpuMsg_GpuSwitched, OnGpuSwitched)
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

  return false;
}

bool GpuChildThread::OnMessageReceived(const IPC::Message& msg) {
  if (ChildThreadImpl::OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChildThread, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyGpuMemoryBuffer, OnDestroyGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_LoadedShader, OnLoadedShader)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(GpuMsg_WakeUpGpu, OnWakeUpGpu);
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyingVideoSurface,
                        OnDestroyingVideoSurface);
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  return false;
}

bool GpuChildThread::AcceptConnection(shell::Connection* connection) {
  // Use of base::Unretained(this) is safe here because |service_registry()|
  // will be destroyed before GpuChildThread is destructed.
  connection->GetInterfaceRegistry()->AddInterface(base::Bind(
      &GpuChildThread::BindProcessControlRequest, base::Unretained(this)));

  if (GetContentClient()->gpu()) {  // NULL in tests.
    GetContentClient()->gpu()->ExposeInterfacesToBrowser(
        connection->GetInterfaceRegistry());
  }

  // We don't want to process any incoming interface requests until
  // OnInitialize().
  if (!gpu_channel_manager_) {
    connection->GetInterfaceRegistry()->PauseBinding();
    resume_interface_bindings_callback_ = base::Bind(
        &shell::InterfaceRegistry::ResumeBinding,
        connection->GetInterfaceRegistry()->GetWeakPtr());
  }

  return true;
}

shell::InterfaceRegistry* GpuChildThread::GetInterfaceRegistryForConnection() {
  return nullptr;
}

shell::InterfaceProvider* GpuChildThread::GetInterfaceProviderForConnection() {
  return nullptr;
}

void GpuChildThread::SetActiveURL(const GURL& url) {
  GetContentClient()->SetActiveURL(url);
}

void GpuChildThread::DidCreateOffscreenContext(const GURL& active_url) {
  Send(new GpuHostMsg_DidCreateOffscreenContext(active_url));
}

void GpuChildThread::DidDestroyChannel(int client_id) {
  media_service_->RemoveChannel(client_id);
  Send(new GpuHostMsg_DestroyChannel(client_id));
}

void GpuChildThread::DidDestroyOffscreenContext(const GURL& active_url) {
  Send(new GpuHostMsg_DidDestroyOffscreenContext(active_url));
}

void GpuChildThread::DidLoseContext(bool offscreen,
                                    gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  Send(new GpuHostMsg_DidLoseContext(offscreen, reason, active_url));
}

void GpuChildThread::GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) {
  Send(new GpuHostMsg_GpuMemoryUmaStats(params));
}

#if defined(OS_WIN)
void GpuChildThread::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  Send(new GpuHostMsg_AcceleratedSurfaceCreatedChildWindow(parent_window,
                                                           child_window));
}
#endif

void GpuChildThread::StoreShaderToDisk(int32_t client_id,
                                       const std::string& key,
                                       const std::string& shader) {
  Send(new GpuHostMsg_CacheShader(client_id, key, shader));
}

void GpuChildThread::OnInitialize(const gpu::GpuPreferences& gpu_preferences) {
  if (!resume_interface_bindings_callback_.is_null())
    base::ResetAndReturn(&resume_interface_bindings_callback_).Run();

  gpu_preferences_ = gpu_preferences;

  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(
          gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAccelerator::IsSupported();

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
    base::MessageLoop::current()->QuitWhenIdle();
    return;
  }

  // We don't need to pipe log messages if we are running the GPU thread in
  // the browser process.
  if (!in_browser_process_)
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  gpu::SyncPointManager* sync_point_manager = nullptr;
  // Note SyncPointManager from ContentGpuClient cannot be owned by this.
  if (GetContentClient()->gpu())
    sync_point_manager = GetContentClient()->gpu()->GetSyncPointManager();
  if (!sync_point_manager) {
    if (!owned_sync_point_manager_) {
      owned_sync_point_manager_.reset(new gpu::SyncPointManager(false));
    }
    sync_point_manager = owned_sync_point_manager_.get();
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(
      new gpu::GpuChannelManager(gpu_preferences_, this, watchdog_thread_.get(),
                            base::ThreadTaskRunnerHandle::Get().get(),
                            ChildProcess::current()->io_task_runner(),
                            ChildProcess::current()->GetShutDownEvent(),
                            sync_point_manager, gpu_memory_buffer_factory_));

  media_service_.reset(new media::MediaService(gpu_channel_manager_.get()));

#if defined(TOOLKIT_QT)
  gpu_channel_manager_->set_share_group(GetContentClient()->browser()->GetInProcessGpuShareGroup());
#endif

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupport()
      ->OnChannelEstablished(this);
#endif
}

void GpuChildThread::OnFinalize() {
  // Quit the GPU process
  base::MessageLoop::current()->QuitWhenIdle();
}

void GpuChildThread::StopWatchdog() {
  if (watchdog_thread_.get()) {
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnCollectGraphicsInfo() {
  if (dead_on_arrival_)
    return;

#if defined(OS_WIN)
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
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
      DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoNone:
      NOTREACHED();
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
  gpu_info_.dx_diagnostics_info_state = gpu::kCollectInfoSuccess;
#endif  // OS_WIN

  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));

#if defined(OS_WIN)
  if (!in_browser_process_) {
    // The unsandboxed GPU process fulfilled its duty.  Rest in peace.
    base::MessageLoop::current()->QuitWhenIdle();
  }
#endif  // OS_WIN
}

void GpuChildThread::OnGetVideoMemoryUsageStats() {
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  if (gpu_channel_manager_) {
    gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
        &video_memory_usage_stats);
  }
  Send(new GpuHostMsg_VideoMemoryUsageStats(video_memory_usage_stats));
}

void GpuChildThread::OnClean() {
  DVLOG(1) << "GPU: Removing all contexts";
  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyAllChannels();
}

void GpuChildThread::OnCrash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuChildThread::OnHang() {
  DVLOG(1) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

void GpuChildThread::OnDisableWatchdog() {
  DVLOG(1) << "GPU: Disabling watchdog thread";
  if (watchdog_thread_.get()) {
    // Disarm the watchdog before shutting down the message loop. This prevents
    // the future posting of tasks to the message loop.
    if (watchdog_thread_->message_loop())
      watchdog_thread_->PostAcknowledge();
    // Prevent rearming.
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnGpuSwitched() {
  DVLOG(1) << "GPU: GPU has switched";
  // Notify observers in the GPU process.
  if (!in_browser_process_)
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
}

void GpuChildThread::OnEstablishChannel(const EstablishChannelParams& params) {
  if (!gpu_channel_manager_)
    return;

  IPC::ChannelHandle channel_handle = gpu_channel_manager_->EstablishChannel(
      params.client_id, params.client_tracing_id, params.preempts,
      params.allow_view_command_buffers, params.allow_real_time_streams);
  media_service_->AddChannel(params.client_id);
  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuChildThread::OnCloseChannel(int32_t client_id) {
  if (gpu_channel_manager_)
    gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuChildThread::OnLoadedShader(const std::string& shader) {
  if (gpu_channel_manager_)
    gpu_channel_manager_->PopulateShaderCache(shader);
}

void GpuChildThread::OnDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

#if defined(OS_ANDROID)
void GpuChildThread::OnWakeUpGpu() {
  if (gpu_channel_manager_)
    gpu_channel_manager_->WakeUpGpu();
}

void GpuChildThread::OnDestroyingVideoSurface(int surface_id) {
  media::AVDASurfaceTracker::GetInstance()->NotifyDestroyingSurface(surface_id);
  Send(new GpuHostMsg_DestroyingVideoSurfaceAck(surface_id));
}
#endif

void GpuChildThread::OnLoseAllContexts() {
  if (gpu_channel_manager_) {
    gpu_channel_manager_->DestroyAllChannels();
    media_service_->DestroyAllChannels();
  }
}

void GpuChildThread::BindProcessControlRequest(
    mojo::InterfaceRequest<mojom::ProcessControl> request) {
  DVLOG(1) << "GPU: Binding ProcessControl request";
  DCHECK(process_control_);
  process_control_bindings_.AddBinding(process_control_.get(),
                                       std::move(request));
}

}  // namespace content
