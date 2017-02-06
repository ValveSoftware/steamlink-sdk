// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "components/tracing/child/child_trace_message_filter.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/child_process.h"
#include "content/child/child_resource_message_filter.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/memory/child_memory_message_filter.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/power_monitor_broadcast_source.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/service_worker/service_worker_message_filter.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/websocket_dispatcher.h"
#include "content/child/websocket_message_filter.h"
#include "content/common/child_process_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/mojo_shell_connection.h"
#include "ipc/attachment_broker.h"
#include "ipc/attachment_broker_unprivileged.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "services/shell/runner/common/client_util.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#endif

using tracked_objects::ThreadData;

namespace content {
namespace {

// How long to wait for a connection to the browser process before giving up.
const int kConnectionTimeoutS = 15;

base::LazyInstance<base::ThreadLocalPointer<ChildThreadImpl> > g_lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

// This isn't needed on Windows because there the sandbox's job object
// terminates child processes automatically. For unsandboxed processes (i.e.
// plugins), PluginThread has EnsureTerminateMessageFilter.
#if defined(OS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// A thread delegate that waits for |duration| and then exits the process with
// _exit(0).
class WaitAndExitDelegate : public base::PlatformThread::Delegate {
 public:
  explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

  void ThreadMain() override {
    base::PlatformThread::Sleep(duration_);
    _exit(0);
  }

 private:
  const base::TimeDelta duration_;
  DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelegate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelegate> delegate(
      new WaitAndExitDelegate(duration));

  const bool thread_created =
      base::PlatformThread::CreateNonJoinable(0, delegate.get());
  if (!thread_created)
    return false;

  // A non joinable thread has been created. The thread will either terminate
  // the process or will be terminated by the process. Therefore, keep the
  // delegate object alive for the lifetime of the process.
  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  ignore_result(leaking_delegate);
  return true;
}
#endif

class SuicideOnChannelErrorFilter : public IPC::MessageFilter {
 public:
  // IPC::MessageFilter
  void OnChannelError() override {
    // For renderer/worker processes:
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a renderer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (FrameMsg_BeforeUnload and the
    // error from the IPC sender) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the browser SIGKILL the renderers, but that leaves open a
    // large window where a browser failure (or a user, manually terminating
    // the browser because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the sender so that we can process this event
    // here and kill the process.
    base::debug::StopProfiling();
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // Some sanitizer tools rely on exit handlers (e.g. to run leak detection,
    // or dump code coverage data to disk). Instead of exiting the process
    // immediately, we give it 60 seconds to run exit handlers.
    CHECK(CreateWaitAndExitThread(base::TimeDelta::FromSeconds(60)));
#if defined(LEAK_SANITIZER)
    // Invoke LeakSanitizer early to avoid detecting shutdown-only leaks. If
    // leaks are found, the process will exit here.
    __lsan_do_leak_check();
#endif
#else
    _exit(0);
#endif
  }

 protected:
  ~SuicideOnChannelErrorFilter() override {}
};

#endif  // OS(POSIX)

#if defined(OS_ANDROID)
// A class that allows for triggering a clean shutdown from another
// thread through draining the main thread's msg loop.
class QuitClosure {
 public:
  QuitClosure();
  ~QuitClosure();

  void BindToMainThread();
  void PostQuitFromNonMainThread();

 private:
  static void PostClosure(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::Closure closure);

  base::Lock lock_;
  base::ConditionVariable cond_var_;
  base::Closure closure_;
};

QuitClosure::QuitClosure() : cond_var_(&lock_) {
}

QuitClosure::~QuitClosure() {
}

void QuitClosure::PostClosure(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::Closure closure) {
  task_runner->PostTask(FROM_HERE, closure);
}

void QuitClosure::BindToMainThread() {
  base::AutoLock lock(lock_);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());
  base::Closure quit_closure =
      base::MessageLoop::current()->QuitWhenIdleClosure();
  closure_ = base::Bind(&QuitClosure::PostClosure, task_runner, quit_closure);
  cond_var_.Signal();
}

void QuitClosure::PostQuitFromNonMainThread() {
  base::AutoLock lock(lock_);
  while (closure_.is_null())
    cond_var_.Wait();

  closure_.Run();
}

base::LazyInstance<QuitClosure> g_quit_closure = LAZY_INSTANCE_INITIALIZER;
#endif

void InitializeMojoIPCChannel() {
  mojo::edk::ScopedPlatformHandle platform_channel;
#if defined(OS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      mojo::edk::PlatformChannelPair::kMojoPlatformChannelHandleSwitch)) {
    platform_channel =
        mojo::edk::PlatformChannelPair::PassClientHandleFromParentProcess(
            *base::CommandLine::ForCurrentProcess());
  } else {
    // If this process is elevated, it will have a pipe path passed on the
    // command line.
    platform_channel =
        mojo::edk::NamedPlatformChannelPair::PassClientHandleFromParentProcess(
            *base::CommandLine::ForCurrentProcess());
  }
#elif defined(OS_POSIX)
  platform_channel.reset(mojo::edk::PlatformHandle(
      base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel)));
#endif
  // Mojo isn't supported on all child process types.
  // TODO(crbug.com/604282): Support Mojo in the remaining processes.
  if (!platform_channel.is_valid())
    return;
  mojo::edk::SetParentPipeHandle(std::move(platform_channel));
}

}  // namespace

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

ChildThreadImpl::Options::Options()
    : channel_name(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID)),
      use_mojo_channel(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMojoChannelToken)) {
}

ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() {
}

ChildThreadImpl::Options::Builder::Builder() {
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(
    const InProcessChildThreadParams& params) {
  options_.browser_process_io_runner = params.io_runner();
  options_.channel_name = params.channel_name();
  options_.in_process_ipc_token = params.ipc_token();
  options_.in_process_application_token = params.application_token();
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::UseMojoChannel(bool use_mojo_channel) {
  options_.use_mojo_channel = use_mojo_channel;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::WithChannelName(
    const std::string& channel_name) {
  options_.channel_name = channel_name;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AddStartupFilter(
    IPC::MessageFilter* filter) {
  options_.startup_filters.push_back(filter);
  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadMessageRouter::ChildThreadMessageRouter(
    IPC::Sender* sender)
    : sender_(sender) {}

bool ChildThreadImpl::ChildThreadMessageRouter::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool ChildThreadImpl::ChildThreadMessageRouter::RouteMessage(
    const IPC::Message& msg) {
  bool handled = IPC::MessageRouter::RouteMessage(msg);
#if defined(OS_ANDROID)
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
#endif
  return handled;
}

ChildThreadImpl::ChildThreadImpl()
    : router_(this),
      channel_connected_factory_(this) {
  Init(Options::Builder().Build());
}

ChildThreadImpl::ChildThreadImpl(const Options& options)
    : router_(this),
      browser_process_io_runner_(options.browser_process_io_runner),
      channel_connected_factory_(this) {
  Init(options);
}

scoped_refptr<base::SequencedTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInBrowserProcess())
    return browser_process_io_runner_;
  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::ConnectChannel(bool use_mojo_channel,
                                     const std::string& ipc_token) {
  bool create_pipe_now = true;
  if (use_mojo_channel) {
    VLOG(1) << "Mojo is enabled on child";
    mojo::ScopedMessagePipeHandle handle;
    if (!IsInBrowserProcess()) {
      DCHECK(!handle.is_valid());
      handle = mojo::edk::CreateChildMessagePipe(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kMojoChannelToken));
    } else {
      handle = mojo::edk::CreateChildMessagePipe(ipc_token);
    }
    DCHECK(handle.is_valid());
    channel_->Init(IPC::ChannelMojo::CreateClientFactory(std::move(handle)),
                   create_pipe_now);
    return;
  }

  VLOG(1) << "Mojo is disabled on child";
  channel_->Init(channel_name_, IPC::Channel::MODE_CLIENT, create_pipe_now);
}

void ChildThreadImpl::Init(const Options& options) {
  channel_name_ = options.channel_name;

  g_lazy_tls.Pointer()->Set(this);
  on_channel_error_called_ = false;
  message_loop_ = base::MessageLoop::current();
#ifdef IPC_MESSAGE_LOG_ENABLED
  // We must make sure to instantiate the IPC Logger *before* we create the
  // channel, otherwise we can get a callback on the IO thread which creates
  // the logger, and the logger does not like being created on the IO thread.
  IPC::Logging::GetInstance();
#endif

  IPC::AttachmentBrokerUnprivileged::CreateBrokerIfNeeded();

  channel_ =
      IPC::SyncChannel::Create(this, ChildProcess::current()->io_task_runner(),
                               ChildProcess::current()->GetShutDownEvent());
#ifdef IPC_MESSAGE_LOG_ENABLED
  if (!IsInBrowserProcess())
    IPC::Logging::GetInstance()->SetIPCSender(this);
#endif

  if (!IsInBrowserProcess()) {
    // Don't double-initialize IPC support in single-process mode.
    mojo_ipc_support_.reset(new mojo::edk::ScopedIPCSupport(GetIOTaskRunner()));
    InitializeMojoIPCChannel();
  }
  std::string mojo_application_token;
  if (!IsInBrowserProcess()) {
    mojo_application_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kMojoApplicationChannelToken);
  } else {
    mojo_application_token = options.in_process_application_token;
  }
  if (!mojo_application_token.empty()) {
    mojo::ScopedMessagePipeHandle handle =
        mojo::edk::CreateChildMessagePipe(mojo_application_token);
    DCHECK(handle.is_valid());
    mojo_shell_connection_ = MojoShellConnection::Create(
        mojo::MakeRequest<shell::mojom::ShellClient>(std::move(handle)));
    mojo_shell_connection_->AddEmbeddedShellClient(this);
  }

  sync_message_filter_ = channel_->CreateSyncMessageFilter();
  thread_safe_sender_ = new ThreadSafeSender(
      message_loop_->task_runner(), sync_message_filter_.get());

  resource_dispatcher_.reset(new ResourceDispatcher(
      this, message_loop()->task_runner()));
  websocket_dispatcher_.reset(new WebSocketDispatcher);
  file_system_dispatcher_.reset(new FileSystemDispatcher());

  histogram_message_filter_ = new ChildHistogramMessageFilter();
  resource_message_filter_ =
      new ChildResourceMessageFilter(resource_dispatcher());

  service_worker_message_filter_ =
      new ServiceWorkerMessageFilter(thread_safe_sender_.get());

  quota_message_filter_ =
      new QuotaMessageFilter(thread_safe_sender_.get());
  quota_dispatcher_.reset(new QuotaDispatcher(thread_safe_sender_.get(),
                                              quota_message_filter_.get()));
  notification_dispatcher_ =
      new NotificationDispatcher(thread_safe_sender_.get());
  push_dispatcher_ = new PushDispatcher(thread_safe_sender_.get());

  websocket_message_filter_ =
      new WebSocketMessageFilter(websocket_dispatcher_.get());

  channel_->AddFilter(histogram_message_filter_.get());
  channel_->AddFilter(resource_message_filter_.get());
  channel_->AddFilter(quota_message_filter_->GetFilter());
  channel_->AddFilter(notification_dispatcher_->GetFilter());
  channel_->AddFilter(push_dispatcher_->GetFilter());
  channel_->AddFilter(service_worker_message_filter_->GetFilter());
  channel_->AddFilter(websocket_message_filter_.get());

  if (!IsInBrowserProcess()) {
    // In single process mode, browser-side tracing and memory will cover the
    // whole process including renderers.
    channel_->AddFilter(new tracing::ChildTraceMessageFilter(
        ChildProcess::current()->io_task_runner()));
    channel_->AddFilter(new ChildMemoryMessageFilter());
  }

  // In single process mode we may already have a power monitor
  if (!base::PowerMonitor::Get()) {
    std::unique_ptr<PowerMonitorBroadcastSource> power_monitor_source(
        new PowerMonitorBroadcastSource());
    channel_->AddFilter(power_monitor_source->GetMessageFilter());

    power_monitor_.reset(
        new base::PowerMonitor(std::move(power_monitor_source)));
  }

#if defined(OS_POSIX)
  // Check that --process-type is specified so we don't do this in unit tests
  // and single-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    channel_->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  // Add filters passed here via options.
  for (auto startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  IPC::AttachmentBroker* broker = IPC::AttachmentBroker::GetGlobal();
  if (broker && !broker->IsPrivilegedBroker())
    broker->RegisterBrokerCommunicationChannel(channel_.get());
  ConnectChannel(options.use_mojo_channel, options.in_process_ipc_token);

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);
  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  message_loop_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChildThreadImpl::EnsureConnected,
                            channel_connected_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(connection_timeout));

#if defined(OS_ANDROID)
  g_quit_closure.Get().BindToMainThread();
#endif

  shared_bitmap_manager_.reset(
      new ChildSharedBitmapManager(thread_safe_sender()));

  gpu_memory_buffer_manager_.reset(
      new ChildGpuMemoryBufferManager(thread_safe_sender()));

  discardable_shared_memory_manager_.reset(
      new ChildDiscardableSharedMemoryManager(thread_safe_sender()));
}

ChildThreadImpl::~ChildThreadImpl() {
  if (MojoShellConnection::GetForProcess())
    MojoShellConnection::DestroyForProcess();

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  IPC::AttachmentBroker* broker = IPC::AttachmentBroker::GetGlobal();
  if (broker && !broker->IsPrivilegedBroker())
    broker->DeregisterBrokerCommunicationChannel(channel_.get());

  channel_->RemoveFilter(histogram_message_filter_.get());
  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCTaskRunner();
  g_lazy_tls.Pointer()->Set(NULL);
}

void ChildThreadImpl::Shutdown() {
  // Delete objects that hold references to blink so derived classes can
  // safely shutdown blink in their Shutdown implementation.
  file_system_dispatcher_.reset();
  quota_dispatcher_.reset();
  WebFileSystemImpl::DeleteThreadSpecificInstance();
}

void ChildThreadImpl::ShutdownDiscardableSharedMemoryManager() {
  discardable_shared_memory_manager_.reset();
}

void ChildThreadImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_factory_.InvalidateWeakPtrs();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(base::MessageLoop::current() == message_loop());
  if (!channel_) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

#if defined(OS_WIN)
void ChildThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void ChildThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}
#endif

void ChildThreadImpl::RecordAction(const base::UserMetricsAction& action) {
    NOTREACHED();
}

void ChildThreadImpl::RecordComputedAction(const std::string& action) {
    NOTREACHED();
}

MojoShellConnection* ChildThreadImpl::GetMojoShellConnection() {
  return mojo_shell_connection_.get();
}

shell::InterfaceRegistry* ChildThreadImpl::GetInterfaceRegistry() {
  if (!interface_registry_.get())
    interface_registry_.reset(new shell::InterfaceRegistry(nullptr));
  return interface_registry_.get();
}

shell::InterfaceProvider* ChildThreadImpl::GetRemoteInterfaces() {
  if (!remote_interfaces_.get())
    remote_interfaces_.reset(new shell::InterfaceProvider);
  return remote_interfaces_.get();
}

shell::InterfaceRegistry* ChildThreadImpl::GetInterfaceRegistryForConnection() {
  return GetInterfaceRegistry();
}

shell::InterfaceProvider* ChildThreadImpl::GetInterfaceProviderForConnection() {
  return GetRemoteInterfaces();
}

IPC::MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(base::MessageLoop::current() == message_loop());
  return &router_;
}

std::unique_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size) {
  DCHECK(base::MessageLoop::current() == message_loop());
  return AllocateSharedMemory(buf_size, this, nullptr);
}

// static
std::unique_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size,
    IPC::Sender* sender,
    bool* out_of_memory) {
  std::unique_ptr<base::SharedMemory> shared_buf;
  // Ask the browser to create the shared memory, since this is blocked by the
  // sandbox on most platforms.
  base::SharedMemoryHandle shared_mem_handle;
  if (sender->Send(new ChildProcessHostMsg_SyncAllocateSharedMemory(
          buf_size, &shared_mem_handle))) {
    if (base::SharedMemory::IsHandleValid(shared_mem_handle)) {
      shared_buf.reset(new base::SharedMemory(shared_mem_handle, false));
    } else {
      LOG(WARNING) << "Browser failed to allocate shared memory";
      if (out_of_memory)
        *out_of_memory = true;
      return nullptr;
    }
  } else {
    // Send is allowed to fail during shutdown. Return null in this case.
    if (out_of_memory)
      *out_of_memory = false;
    return nullptr;
  }
  return shared_buf;
}

bool ChildThreadImpl::OnMessageReceived(const IPC::Message& msg) {
  // Resource responses are sent to the resource dispatcher.
  if (resource_dispatcher_->OnMessageReceived(msg))
    return true;
  if (file_system_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildThreadImpl, msg)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_Shutdown, OnShutdown)
#if defined(IPC_MESSAGE_LOG_ENABLED)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetIPCLoggingEnabled,
                        OnSetIPCLoggingEnabled)
#endif
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetProfilerStatus,
                        OnSetProfilerStatus)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetChildProfilerData,
                        OnGetChildProfilerData)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_ProfilingPhaseCompleted,
                        OnProfilingPhaseCompleted)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetProcessBackgrounded,
                        OnProcessBackgrounded)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_PurgeAndSuspend,
                        OnProcessPurgeAndSuspend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

bool ChildThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildThreadImpl::OnProcessBackgrounded(bool backgrounded) {
  // Set timer slack to maximum on main thread when in background.
  base::TimerSlack timer_slack = base::TIMER_SLACK_NONE;
  if (backgrounded)
    timer_slack = base::TIMER_SLACK_MAXIMUM;
  base::MessageLoop::current()->SetTimerSlack(timer_slack);
}

void ChildThreadImpl::OnProcessPurgeAndSuspend() {
}

void ChildThreadImpl::OnShutdown() {
  base::MessageLoop::current()->QuitWhenIdle();
}

#if defined(IPC_MESSAGE_LOG_ENABLED)
void ChildThreadImpl::OnSetIPCLoggingEnabled(bool enable) {
  if (enable)
    IPC::Logging::GetInstance()->Enable();
  else
    IPC::Logging::GetInstance()->Disable();
}
#endif  //  IPC_MESSAGE_LOG_ENABLED

void ChildThreadImpl::OnSetProfilerStatus(ThreadData::Status status) {
  ThreadData::InitializeAndSetTrackingStatus(status);
}

void ChildThreadImpl::OnGetChildProfilerData(int sequence_number,
                                             int current_profiling_phase) {
  tracked_objects::ProcessDataSnapshot process_data;
  ThreadData::Snapshot(current_profiling_phase, &process_data);

  Send(
      new ChildProcessHostMsg_ChildProfilerData(sequence_number, process_data));
}

void ChildThreadImpl::OnProfilingPhaseCompleted(int profiling_phase) {
  ThreadData::OnProfilingPhaseCompleted(profiling_phase);
}

ChildThreadImpl* ChildThreadImpl::current() {
  return g_lazy_tls.Pointer()->Get();
}

#if defined(OS_ANDROID)
// The method must NOT be called on the child thread itself.
// It may block the child thread if so.
void ChildThreadImpl::ShutdownThread() {
  DCHECK(!ChildThreadImpl::current()) <<
      "this method should NOT be called from child thread itself";
  g_quit_closure.Get().PostQuitFromNonMainThread();
}
#endif

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_)
    return;

  // The child process shutdown sequence is a request response based mechanism,
  // where we send out an initial feeler request to the child process host
  // instance in the browser to verify if it's ok to shutdown the child process.
  // The browser then sends back a response if it's ok to shutdown. This avoids
  // race conditions if the process refcount is 0 but there's an IPC message
  // inflight that would addref it.
  Send(new ChildProcessHostMsg_ShutdownRequest);
}

void ChildThreadImpl::EnsureConnected() {
  VLOG(0) << "ChildThreadImpl::EnsureConnected()";
  base::Process::Current().Terminate(0, false);
}

bool ChildThreadImpl::IsInBrowserProcess() const {
  return static_cast<bool>(browser_process_io_runner_);
}

}  // namespace content
