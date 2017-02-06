// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/histogram_message_filter.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "ipc/attachment_broker.h"
#include "ipc/attachment_broker_privileged.h"
#include "mojo/edk/embedder/embedder.h"

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

namespace content {
namespace {

static base::LazyInstance<BrowserChildProcessHostImpl::BrowserChildProcessList>
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<BrowserChildProcessObserver>>
    g_observers = LAZY_INSTANCE_INITIALIZER;

void NotifyProcessLaunchedAndConnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessLaunchedAndConnected(data));
}

void NotifyProcessHostConnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostConnected(data));
}

void NotifyProcessHostDisconnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostDisconnected(data));
}

void NotifyProcessCrashed(const ChildProcessData& data, int exit_code) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessCrashed(data, exit_code));
}

void NotifyProcessKilled(const ChildProcessData& data, int exit_code) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessKilled(data, exit_code));
}

}  // namespace

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate) {
  return new BrowserChildProcessHostImpl(
      process_type, delegate, mojo::edk::GenerateRandomToken());
}

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    const std::string& mojo_child_token) {
  return new BrowserChildProcessHostImpl(
      process_type, delegate, mojo_child_token);
}

BrowserChildProcessHost* BrowserChildProcessHost::FromID(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserChildProcessHostImpl::BrowserChildProcessList* process_list =
      g_child_process_list.Pointer();
  for (BrowserChildProcessHostImpl* host : *process_list) {
    if (host->GetData().id == child_process_id)
      return host;
  }
  return nullptr;
}

#if defined(OS_MACOSX)
base::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return MachBroker::GetInstance();
}
#endif

// static
BrowserChildProcessHostImpl::BrowserChildProcessList*
    BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

// static
void BrowserChildProcessHostImpl::AddObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_observers.Get().AddObserver(observer);
}

// static
void BrowserChildProcessHostImpl::RemoveObserver(
    BrowserChildProcessObserver* observer) {
  // TODO(phajdan.jr): Check thread after fixing http://crbug.com/167126.
  g_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    const std::string& mojo_child_token)
    : data_(process_type),
      delegate_(delegate),
      mojo_child_token_(mojo_child_token),
      power_monitor_message_broadcaster_(this),
      is_channel_connected_(false),
      notify_child_disconnected_(false),
      weak_factory_(this) {
  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

#if USE_ATTACHMENT_BROKER
  // Construct the privileged attachment broker early in the life cycle of a
  // child process. This ensures that when a test is being run in one of the
  // single process modes, the global attachment broker is the privileged
  // attachment broker, rather than an unprivileged attachment broker.
#if defined(OS_MACOSX)
  IPC::AttachmentBrokerPrivileged::CreateBrokerIfNeeded(
      MachBroker::GetInstance());
#else
  IPC::AttachmentBrokerPrivileged::CreateBrokerIfNeeded();
#endif  // defined(OS_MACOSX)
#endif  // USE_ATTACHMENT_BROKER

  child_process_host_.reset(ChildProcessHost::Create(this));
  AddFilter(new TraceMessageFilter(data_.id));
  AddFilter(new ProfilerMessageFilter(process_type));
  AddFilter(new HistogramMessageFilter);
  AddFilter(new MemoryMessageFilter(this, process_type));

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);

  power_monitor_message_broadcaster_.Init();
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);

  if (notify_child_disconnected_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&NotifyProcessHostDisconnected, data_));
  }
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (BrowserChildProcessList::iterator it = copy.begin();
       it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

// static
void BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(
    base::CommandLine* cmd_line) {
  std::string enabled_features;
  std::string disabled_features;
  base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                        &disabled_features);
  if (!enabled_features.empty())
    cmd_line->AppendSwitchASCII(switches::kEnableFeatures, enabled_features);
  if (!disabled_features.empty())
    cmd_line->AppendSwitchASCII(switches::kDisableFeatures, disabled_features);

  // If we run base::FieldTrials, we want to pass to their state to the
  // child process so that it can act in accordance with each state.
  std::string field_trial_states;
  base::FieldTrialList::AllStatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    cmd_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                field_trial_states);
  }
}

void BrowserChildProcessHostImpl::Launch(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    bool terminate_on_shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, data_.id);

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
    switches::kDisableLogging,
    switches::kEnableLogging,
    switches::kIPCConnectionTimeout,
    switches::kLoggingLevel,
    switches::kTraceToConsole,
    switches::kV,
    switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches,
                             arraysize(kForwardSwitches));

  notify_child_disconnected_ = true;
  child_process_.reset(new ChildProcessLauncher(
      delegate,
      cmd_line,
      data_.id,
      this,
      mojo_child_token_,
      base::Bind(&BrowserChildProcessHostImpl::OnMojoError,
                 weak_factory_.GetWeakPtr(),
                 base::ThreadTaskRunnerHandle::Get()),
      terminate_on_shutdown));
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return child_process_host_.get();
}

const base::Process& BrowserChildProcessHostImpl::GetProcess() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetProcess().IsValid())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetProcess();
}

void BrowserChildProcessHostImpl::SetName(const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetHandle(base::ProcessHandle handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.handle = handle;
}

shell::InterfaceRegistry* BrowserChildProcessHostImpl::GetInterfaceRegistry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return delegate_->GetInterfaceRegistry();
}

shell::InterfaceProvider* BrowserChildProcessHostImpl::GetRemoteInterfaces() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return delegate_->GetRemoteInterfaces();
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

void BrowserChildProcessHostImpl::SetBackgrounded(bool backgrounded) {
  child_process_->SetProcessBackgrounded(backgrounded);
}

void BrowserChildProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  child_process_host_->AddFilter(filter->GetFilter());
}

void BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
    int process_type) {
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.BadMessgeTerminated", process_type,
                            PROCESS_TYPE_MAX);
}

base::TerminationStatus BrowserChildProcessHostImpl::GetTerminationStatus(
    bool known_dead, int* exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!child_process_)  // If the delegate doesn't use Launch() helper.
    return base::GetTerminationStatus(data_.handle, exit_code);
  return child_process_->GetChildTerminationStatus(known_dead,
                                                   exit_code);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  is_channel_connected_ = true;
  notify_child_disconnected_ = true;

#if defined(OS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel.
  early_exit_watcher_.StopWatching();
#endif

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyProcessHostConnected, data_));

  delegate_->OnChannelConnected(peer_pid);

  if (IsProcessLaunched()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&NotifyProcessLaunchedAndConnected,
                                       data_));
  }
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

void BrowserChildProcessHostImpl::OnBadMessageReceived(
    const IPC::Message& message) {
  TerminateOnBadMessageReceived(message.type());
}

void BrowserChildProcessHostImpl::TerminateOnBadMessageReceived(uint32_t type) {
  HistogramBadMessageTerminated(data_.process_type);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad IPC message of type "
             << type;

  // Create a memory dump. This will contain enough stack frames to work out
  // what the bad message was.
  base::debug::DumpWithoutCrashing();

  child_process_->GetProcess().Terminate(RESULT_CODE_KILLED_BAD_MESSAGE, false);
}

bool BrowserChildProcessHostImpl::CanShutdown() {
  return delegate_->CanShutdown();
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_WIN)
  // OnChildDisconnected may be called without OnChannelConnected, so stop the
  // early exit watcher so GetTerminationStatus can close the process handle.
  early_exit_watcher_.StopWatching();
#endif
  if (child_process_.get() || data_.handle) {
    int exit_code;
    base::TerminationStatus status = GetTerminationStatus(
        true /* known_dead */, &exit_code);
    switch (status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
        delegate_->OnProcessCrashed(exit_code);
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&NotifyProcessCrashed, data_, exit_code));
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
        break;
      }
#if defined(OS_ANDROID)
      case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
        delegate_->OnProcessCrashed(exit_code);
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&NotifyProcessKilled, data_, exit_code));
        // Report that this child process was killed.
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
        break;
      }
      case base::TERMINATION_STATUS_STILL_RUNNING: {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
      }
      default:
        break;
    }
    UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected2",
                              data_.process_type,
                              PROCESS_TYPE_MAX);
#if defined(OS_CHROMEOS)
    if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM) {
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2.OOM",
                                data_.process_type,
                                PROCESS_TYPE_MAX);
    }
#endif
  }
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  delegate_->OnProcessLaunchFailed(error_code);
  notify_child_disconnected_ = false;
  delete delegate_;  // Will delete us
}

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::Process& process = child_process_->GetProcess();
  DCHECK(process.IsValid());

#if defined(OS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedObject());
  early_exit_watcher_.StartWatchingOnce(process.Handle(), this);
#endif

  // TODO(rvargas) crbug.com/417532: Don't store a handle.
  data_.handle = process.Handle();
  delegate_->OnProcessLaunched();

  if (is_channel_connected_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&NotifyProcessLaunchedAndConnected,
                                       data_));
  }
}

bool BrowserChildProcessHostImpl::IsProcessLaunched() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return child_process_.get() && child_process_->GetProcess().IsValid();
}

// static
void BrowserChildProcessHostImpl::OnMojoError(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& error) {
  if (!task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(
        FROM_HERE, base::Bind(&BrowserChildProcessHostImpl::OnMojoError,
                              process, task_runner, error));
  }
  if (!process)
    return;
  HistogramBadMessageTerminated(process->data_.process_type);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad Mojo message: " << error;

  // Create a memory dump with the error message aliased. This will make it easy
  // to determine details about what interface call failed.
  base::debug::Alias(&error);
  base::debug::DumpWithoutCrashing();
  process->child_process_->GetProcess().Terminate(
      RESULT_CODE_KILLED_BAD_MESSAGE, false);
}

#if defined(OS_WIN)

void BrowserChildProcessHostImpl::OnObjectSignaled(HANDLE object) {
  OnChildDisconnected();
}

#endif

}  // namespace content
