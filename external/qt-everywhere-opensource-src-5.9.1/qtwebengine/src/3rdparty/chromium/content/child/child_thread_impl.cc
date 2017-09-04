// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
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
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/tracing/child/child_trace_message_filter.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/child_process.h"
#include "content/child/child_resource_message_filter.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/memory/child_memory_message_filter.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/service_worker/service_worker_message_filter.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/child_process_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "device/power_monitor/public/cpp/power_monitor_broadcast_source.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/runner/common/client_util.h"

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

class ChannelBootstrapFilter : public ConnectionFilter {
 public:
  explicit ChannelBootstrapFilter(IPC::mojom::ChannelBootstrapPtrInfo bootstrap)
      : bootstrap_(std::move(bootstrap)), weak_factory_(this) {}

 private:
  // ConnectionFilter:
  bool OnConnect(const service_manager::Identity& remote_identity,
                 service_manager::InterfaceRegistry* registry,
                 service_manager::Connector* connector) override {
    if (remote_identity.name() != mojom::kBrowserServiceName)
      return false;

    registry->AddInterface(base::Bind(&ChannelBootstrapFilter::CreateBootstrap,
                                      weak_factory_.GetWeakPtr()));
    return true;
  }

  void CreateBootstrap(IPC::mojom::ChannelBootstrapRequest request) {
    DCHECK(bootstrap_.is_valid());
    mojo::FuseInterface(std::move(request), std::move(bootstrap_));
  }

  IPC::mojom::ChannelBootstrapPtrInfo bootstrap_;
  base::WeakPtrFactory<ChannelBootstrapFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelBootstrapFilter);
};

}  // namespace

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

ChildThreadImpl::Options::Options()
    : auto_start_service_manager_connection(true), connect_to_browser(false) {}

ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() {
}

ChildThreadImpl::Options::Builder::Builder() {
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(
    const InProcessChildThreadParams& params) {
  options_.browser_process_io_runner = params.io_runner();
  options_.in_process_service_request_token = params.service_request_token();
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AutoStartServiceManagerConnection(
    bool auto_start) {
  options_.auto_start_service_manager_connection = auto_start;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ConnectToBrowser(
  bool connect_to_browser) {
  options_.connect_to_browser = connect_to_browser;
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

class ChildThreadImpl::ClientDiscardableSharedMemoryManagerDelegate
    : public discardable_memory::ClientDiscardableSharedMemoryManager::
          Delegate {
 public:
  explicit ClientDiscardableSharedMemoryManagerDelegate(
      scoped_refptr<ThreadSafeSender> sender)
      : sender_(sender) {}
  ~ClientDiscardableSharedMemoryManagerDelegate() override {}

  void AllocateLockedDiscardableSharedMemory(
      size_t size,
      discardable_memory::DiscardableSharedMemoryId id,
      base::SharedMemoryHandle* handle) override {
    sender_->Send(
        new ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory(
            size, id, handle));
  }

  void DeletedDiscardableSharedMemory(
      discardable_memory::DiscardableSharedMemoryId id) override {
    sender_->Send(new ChildProcessHostMsg_DeletedDiscardableSharedMemory(id));
  }

 private:
  scoped_refptr<ThreadSafeSender> sender_;
};

ChildThreadImpl::ChildThreadImpl()
    : route_provider_binding_(this),
      associated_interface_provider_bindings_(
          mojo::BindingSetDispatchMode::WITH_CONTEXT),
      router_(this),
      channel_connected_factory_(
          new base::WeakPtrFactory<ChildThreadImpl>(this)),
      weak_factory_(this) {
  Init(Options::Builder().Build());
}

ChildThreadImpl::ChildThreadImpl(const Options& options)
    : route_provider_binding_(this),
      associated_interface_provider_bindings_(
          mojo::BindingSetDispatchMode::WITH_CONTEXT),
      router_(this),
      browser_process_io_runner_(options.browser_process_io_runner),
      channel_connected_factory_(
          new base::WeakPtrFactory<ChildThreadImpl>(this)),
      weak_factory_(this) {
  Init(options);
}

scoped_refptr<base::SequencedTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInBrowserProcess())
    return browser_process_io_runner_;
  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::ConnectChannel() {
  std::string channel_token;
  mojo::ScopedMessagePipeHandle handle;
  if (!IsInBrowserProcess()) {
    channel_token = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kMojoChannelToken);
  }

  if (!channel_token.empty()) {
    // TODO(rockot): Remove all paths which lead to this branch. The Channel
    // connection should always be established by a service manager connection
    // from the browser. http://crbug.com/623396.
    handle = mojo::edk::CreateChildMessagePipe(channel_token);
  } else {
    DCHECK(service_manager_connection_);
    IPC::mojom::ChannelBootstrapPtr bootstrap;
    handle = mojo::GetProxy(&bootstrap).PassMessagePipe();
    service_manager_connection_->AddConnectionFilter(
        base::MakeUnique<ChannelBootstrapFilter>(bootstrap.PassInterface()));
  }

  DCHECK(handle.is_valid());
  channel_->Init(
      IPC::ChannelMojo::CreateClientFactory(
          std::move(handle), ChildProcess::current()->io_task_runner()),
      true /* create_pipe_now */);
}

void ChildThreadImpl::Init(const Options& options) {
  g_lazy_tls.Pointer()->Set(this);
  on_channel_error_called_ = false;
  message_loop_ = base::MessageLoop::current();
#ifdef IPC_MESSAGE_LOG_ENABLED
  // We must make sure to instantiate the IPC Logger *before* we create the
  // channel, otherwise we can get a callback on the IO thread which creates
  // the logger, and the logger does not like being created on the IO thread.
  IPC::Logging::GetInstance();
#endif

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
  std::string service_request_token;
  if (!IsInBrowserProcess()) {
    service_request_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kServiceRequestChannelToken);
  } else {
    service_request_token = options.in_process_service_request_token;
  }
  if (!service_request_token.empty()) {
    mojo::ScopedMessagePipeHandle handle =
        mojo::edk::CreateChildMessagePipe(service_request_token);
    DCHECK(handle.is_valid());
    service_manager_connection_ = ServiceManagerConnection::Create(
        mojo::MakeRequest<service_manager::mojom::Service>(std::move(handle)),
        GetIOTaskRunner());

    // When connect_to_browser is true, we obtain interfaces from the browser
    // process by connecting to it, rather than from the incoming interface
    // provider. Exposed interfaces are subject to manifest capability spec.
    service_manager::InterfaceProvider* remote_interfaces = nullptr;
    if (options.connect_to_browser) {
      browser_connection_ =
          service_manager_connection_->GetConnector()->Connect(
              mojom::kBrowserServiceName);
    } else {
      remote_interfaces = GetRemoteInterfaces();
    }

    // TODO(rockot): Remove this once all child-to-browser interface connections
    // are made via a Connector rather than directly through an
    // InterfaceProvider, and all exposed interfaces are exposed via a
    // ConnectionFilter.
    service_manager_connection_->SetupInterfaceRequestProxies(
        GetInterfaceRegistry(), remote_interfaces);
  }

  sync_message_filter_ = channel_->CreateSyncMessageFilter();
  thread_safe_sender_ = new ThreadSafeSender(
      message_loop_->task_runner(), sync_message_filter_.get());

  resource_dispatcher_.reset(new ResourceDispatcher(
      this, message_loop()->task_runner()));
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

  channel_->AddFilter(histogram_message_filter_.get());
  channel_->AddFilter(resource_message_filter_.get());
  channel_->AddFilter(quota_message_filter_->GetFilter());
  channel_->AddFilter(notification_dispatcher_->GetFilter());
  channel_->AddFilter(push_dispatcher_->GetFilter());
  channel_->AddFilter(service_worker_message_filter_->GetFilter());

  if (!IsInBrowserProcess()) {
    // In single process mode, browser-side tracing and memory will cover the
    // whole process including renderers.
    channel_->AddFilter(new tracing::ChildTraceMessageFilter(
        ChildProcess::current()->io_task_runner()));
    channel_->AddFilter(new ChildMemoryMessageFilter());
  }

  // In single process mode we may already have a power monitor
  if (!base::PowerMonitor::Get()) {
    std::unique_ptr<device::PowerMonitorBroadcastSource> power_monitor_source(
        new device::PowerMonitorBroadcastSource(GetRemoteInterfaces()));

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
  for (auto* startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  channel_->AddAssociatedInterface(
      base::Bind(&ChildThreadImpl::OnRouteProviderRequest,
                 base::Unretained(this)));

  ConnectChannel();

  // This must always be done after ConnectChannel, because ConnectChannel() may
  // add a ConnectionFilter to the connection.
  if (options.auto_start_service_manager_connection &&
      service_manager_connection_) {
    StartServiceManagerConnection();
  }

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
                            channel_connected_factory_->GetWeakPtr()),
      base::TimeDelta::FromSeconds(connection_timeout));

#if defined(OS_ANDROID)
  g_quit_closure.Get().BindToMainThread();
#endif

  shared_bitmap_manager_.reset(
      new ChildSharedBitmapManager(thread_safe_sender()));

  client_discardable_shared_memory_manager_delegate_ =
      base::MakeUnique<ClientDiscardableSharedMemoryManagerDelegate>(
          thread_safe_sender());
  discardable_shared_memory_manager_ = base::MakeUnique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      client_discardable_shared_memory_manager_delegate_.get());
}

ChildThreadImpl::~ChildThreadImpl() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

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
  channel_connected_factory_.reset();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(message_loop_->task_runner()->BelongsToCurrentThread());
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

ServiceManagerConnection* ChildThreadImpl::GetServiceManagerConnection() {
  return service_manager_connection_.get();
}

service_manager::InterfaceRegistry* ChildThreadImpl::GetInterfaceRegistry() {
  if (!interface_registry_.get()) {
    interface_registry_ = base::MakeUnique<service_manager::InterfaceRegistry>(
        service_manager::mojom::kServiceManager_ConnectorSpec);
  }
  return interface_registry_.get();
}

service_manager::InterfaceProvider* ChildThreadImpl::GetRemoteInterfaces() {
  if (browser_connection_)
    return browser_connection_->GetRemoteInterfaces();

  if (!remote_interfaces_.get())
    remote_interfaces_.reset(new service_manager::InterfaceProvider);
  return remote_interfaces_.get();
}

const service_manager::ServiceInfo&
    ChildThreadImpl::GetChildServiceInfo() const {
  DCHECK(IsConnectedToBrowser());
  return child_info_;
}

const service_manager::ServiceInfo&
    ChildThreadImpl::GetBrowserServiceInfo() const {
  DCHECK(IsConnectedToBrowser());
  return browser_info_;
}

bool ChildThreadImpl::IsConnectedToBrowser() const {
  return connected_to_browser_;
}

IPC::MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(message_loop_->task_runner()->BelongsToCurrentThread());
  return &router_;
}

mojom::RouteProvider* ChildThreadImpl::GetRemoteRouteProvider() {
  if (!remote_route_provider_) {
    DCHECK(channel_);
    channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  }
  return remote_route_provider_.get();
}

std::unique_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size) {
  DCHECK(message_loop_->task_runner()->BelongsToCurrentThread());
  return AllocateSharedMemory(buf_size, this, nullptr);
}

// static
std::unique_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size,
    IPC::Sender* sender,
    bool* out_of_memory) {
  mojo::ScopedSharedBufferHandle mojo_buf =
      mojo::SharedBufferHandle::Create(buf_size);
  if (!mojo_buf->is_valid()) {
    LOG(WARNING) << "Browser failed to allocate shared memory";
    if (out_of_memory)
      *out_of_memory = true;
    return nullptr;
  }

  base::SharedMemoryHandle shared_buf;
  if (mojo::UnwrapSharedMemoryHandle(std::move(mojo_buf), &shared_buf,
                                     nullptr, nullptr) != MOJO_RESULT_OK) {
    LOG(WARNING) << "Browser failed to allocate shared memory";
    return nullptr;
  }

  return base::MakeUnique<base::SharedMemory>(shared_buf, false);
}

#if defined(OS_LINUX)
void ChildThreadImpl::SetThreadPriority(base::PlatformThreadId id,
                                        base::ThreadPriority priority) {
  Send(new ChildProcessHostMsg_SetThreadPriority(id, priority));
}
#endif

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
    IPC_MESSAGE_HANDLER(ChildProcessMsg_Resume, OnProcessResume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

void ChildThreadImpl::StartServiceManagerConnection() {
  DCHECK(service_manager_connection_);
  service_manager_connection_->Start();
  // We don't care about storing the id, since if this pipe closes we're toast.
  service_manager_connection_->AddOnConnectHandler(
      base::Bind(&ChildThreadImpl::OnServiceConnect,
                 weak_factory_.GetWeakPtr()));
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

void ChildThreadImpl::OnProcessResume() {}

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

void ChildThreadImpl::OnRouteProviderRequest(
    mojom::RouteProviderAssociatedRequest request) {
  DCHECK(!route_provider_binding_.is_bound());
  route_provider_binding_.Bind(std::move(request));
}

void ChildThreadImpl::GetRoute(
    int32_t routing_id,
    mojom::AssociatedInterfaceProviderAssociatedRequest request) {
  associated_interface_provider_bindings_.AddBinding(
      this, std::move(request),
      reinterpret_cast<void*>(static_cast<uintptr_t>(routing_id)));
}

void ChildThreadImpl::GetAssociatedInterface(
    const std::string& name,
    mojom::AssociatedInterfaceAssociatedRequest request) {
  int32_t routing_id = static_cast<int32_t>(reinterpret_cast<uintptr_t>(
      associated_interface_provider_bindings_.dispatch_context()));
  Listener* route = router_.GetRoute(routing_id);
  if (route)
    route->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

void ChildThreadImpl::OnServiceConnect(
    const service_manager::ServiceInfo& local_info,
    const service_manager::ServiceInfo& remote_info) {
  if (remote_info.identity.name() != mojom::kBrowserServiceName)
    return;
  DCHECK(!connected_to_browser_);
  connected_to_browser_ = true;
  child_info_ = local_info;
  browser_info_ = remote_info;
}

bool ChildThreadImpl::IsInBrowserProcess() const {
  return static_cast<bool>(browser_process_io_runner_);
}

}  // namespace content
