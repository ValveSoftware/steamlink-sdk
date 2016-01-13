// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>  // For std::pair.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

#if defined(OS_WIN)
#include "base/files/file_path.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/sandbox_init.h"
#elif defined(OS_MACOSX)
#include "content/browser/bootstrap_sandbox_mac.h"
#include "content/browser/mach_broker_mac.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#elif defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/child_process_launcher_android.h"
#elif defined(OS_POSIX)
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/metrics/stats_table.h"
#include "base/posix/global_descriptors.h"
#endif

namespace content {

// Having the functionality of ChildProcessLauncher be in an internal
// ref counted object allows us to automatically terminate the process when the
// parent class destructs, while still holding on to state that we need.
class ChildProcessLauncher::Context
    : public base::RefCountedThreadSafe<ChildProcessLauncher::Context> {
 public:
  Context()
      : client_(NULL),
        client_thread_id_(BrowserThread::UI),
        termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
        exit_code_(RESULT_CODE_NORMAL_EXIT),
        starting_(true),
        // TODO(earthdok): Re-enable on CrOS http://crbug.com/360622
#if (defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER)) && !defined(OS_CHROMEOS)
        terminate_child_on_shutdown_(false)
#else
        terminate_child_on_shutdown_(true)
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
        , zygote_(false)
#endif
        {
  }

  void Launch(
      SandboxedProcessLauncherDelegate* delegate,
      CommandLine* cmd_line,
      int child_process_id,
      Client* client) {
    client_ = client;

    CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));

#if defined(OS_ANDROID)
    // We need to close the client end of the IPC channel to reliably detect
    // child termination. We will close this fd after we create the child
    // process which is asynchronous on Android.
    ipcfd_ = delegate->GetIpcFd();
#endif
    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(
            &Context::LaunchInternal,
            make_scoped_refptr(this),
            client_thread_id_,
            child_process_id,
            delegate,
            cmd_line));
  }

#if defined(OS_ANDROID)
  static void OnChildProcessStarted(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      const base::TimeTicks begin_launch_time,
      base::ProcessHandle handle) {
    RecordHistograms(begin_launch_time);
    if (BrowserThread::CurrentlyOn(client_thread_id)) {
      // This is always invoked on the UI thread which is commonly the
      // |client_thread_id| so we can shortcut one PostTask.
      this_object->Notify(handle);
    } else {
      BrowserThread::PostTask(
          client_thread_id, FROM_HERE,
          base::Bind(
              &ChildProcessLauncher::Context::Notify,
              this_object,
              handle));
    }
  }
#endif

  void ResetClient() {
    // No need for locking as this function gets called on the same thread that
    // client_ would be used.
    CHECK(BrowserThread::CurrentlyOn(client_thread_id_));
    client_ = NULL;
  }

  void set_terminate_child_on_shutdown(bool terminate_on_shutdown) {
    terminate_child_on_shutdown_ = terminate_on_shutdown;
  }

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncher::Context>;
  friend class ChildProcessLauncher;

  ~Context() {
    Terminate();
  }

  static void RecordHistograms(const base::TimeTicks begin_launch_time) {
    base::TimeDelta launch_time = base::TimeTicks::Now() - begin_launch_time;
    if (BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER)) {
      RecordLaunchHistograms(launch_time);
    } else {
      BrowserThread::PostTask(
          BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
          base::Bind(&ChildProcessLauncher::Context::RecordLaunchHistograms,
                     launch_time));
    }
  }

  static void RecordLaunchHistograms(const base::TimeDelta launch_time) {
    // Log the launch time, separating out the first one (which will likely be
    // slower due to the rest of the browser initializing at the same time).
    static bool done_first_launch = false;
    if (done_first_launch) {
      UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchSubsequent", launch_time);
    } else {
      UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchFirst", launch_time);
      done_first_launch = true;
    }
  }

  static void LaunchInternal(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      int child_process_id,
      SandboxedProcessLauncherDelegate* delegate,
      CommandLine* cmd_line) {
    scoped_ptr<SandboxedProcessLauncherDelegate> delegate_deleter(delegate);
#if defined(OS_WIN)
    bool launch_elevated = delegate->ShouldLaunchElevated();
#elif defined(OS_ANDROID)
    int ipcfd = delegate->GetIpcFd();
#elif defined(OS_MACOSX)
    base::EnvironmentMap env = delegate->GetEnvironment();
    int ipcfd = delegate->GetIpcFd();
#elif defined(OS_POSIX)
    bool use_zygote = delegate->ShouldUseZygote();
    base::EnvironmentMap env = delegate->GetEnvironment();
    int ipcfd = delegate->GetIpcFd();
#endif
    scoped_ptr<CommandLine> cmd_line_deleter(cmd_line);
    base::TimeTicks begin_launch_time = base::TimeTicks::Now();

#if defined(OS_WIN)
    base::ProcessHandle handle = base::kNullProcessHandle;
    if (launch_elevated) {
      base::LaunchOptions options;
      options.start_hidden = true;
      base::LaunchElevatedProcess(*cmd_line, options, &handle);
    } else {
      handle = StartSandboxedProcess(delegate, cmd_line);
    }
#elif defined(OS_POSIX)
    std::string process_type =
        cmd_line->GetSwitchValueASCII(switches::kProcessType);
    std::vector<FileDescriptorInfo> files_to_register;
    files_to_register.push_back(
        FileDescriptorInfo(kPrimaryIPCChannel,
                           base::FileDescriptor(ipcfd, false)));
    base::StatsTable* stats_table = base::StatsTable::current();
    if (stats_table &&
        base::SharedMemory::IsHandleValid(
            stats_table->GetSharedMemoryHandle())) {
      files_to_register.push_back(
          FileDescriptorInfo(kStatsTableSharedMemFd,
                             stats_table->GetSharedMemoryHandle()));
    }
#endif

#if defined(OS_ANDROID)
    // Android WebView runs in single process, ensure that we never get here
    // when running in single process mode.
    CHECK(!cmd_line->HasSwitch(switches::kSingleProcess));

    GetContentClient()->browser()->
        GetAdditionalMappedFilesForChildProcess(*cmd_line, child_process_id,
                                                &files_to_register);

    StartChildProcess(cmd_line->argv(), child_process_id, files_to_register,
        base::Bind(&ChildProcessLauncher::Context::OnChildProcessStarted,
                   this_object, client_thread_id, begin_launch_time));

#elif defined(OS_POSIX)
    base::ProcessHandle handle = base::kNullProcessHandle;
    // We need to close the client end of the IPC channel to reliably detect
    // child termination.
    base::ScopedFD ipcfd_closer(ipcfd);

#if !defined(OS_MACOSX)
    GetContentClient()->browser()->
        GetAdditionalMappedFilesForChildProcess(*cmd_line, child_process_id,
                                                &files_to_register);
    if (use_zygote) {
      handle = ZygoteHostImpl::GetInstance()->ForkRequest(cmd_line->argv(),
                                                          files_to_register,
                                                          process_type);
    } else
    // Fall through to the normal posix case below when we're not zygoting.
#endif  // !defined(OS_MACOSX)
    {
      // Convert FD mapping to FileHandleMappingVector
      base::FileHandleMappingVector fds_to_map;
      for (size_t i = 0; i < files_to_register.size(); ++i) {
        fds_to_map.push_back(std::make_pair(
            files_to_register[i].fd.fd,
            files_to_register[i].id +
                base::GlobalDescriptors::kBaseDescriptor));
      }

#if !defined(OS_MACOSX)
      if (process_type == switches::kRendererProcess) {
        const int sandbox_fd =
            RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
        fds_to_map.push_back(std::make_pair(
            sandbox_fd,
            GetSandboxFD()));
      }
#endif  // defined(OS_MACOSX)

      // Actually launch the app.
      base::LaunchOptions options;
      options.environ = env;
      options.fds_to_remap = &fds_to_map;

#if defined(OS_MACOSX)
      // Hold the MachBroker lock for the duration of LaunchProcess. The child
      // will send its task port to the parent almost immediately after startup.
      // The Mach message will be delivered to the parent, but updating the
      // record of the launch will wait until after the placeholder PID is
      // inserted below. This ensures that while the child process may send its
      // port to the parent prior to the parent leaving LaunchProcess, the
      // order in which the record in MachBroker is updated is correct.
      MachBroker* broker = MachBroker::GetInstance();
      broker->GetLock().Acquire();

      // Make sure the MachBroker is running, and inform it to expect a
      // check-in from the new process.
      broker->EnsureRunning();

      const int bootstrap_sandbox_policy = delegate->GetSandboxType();
      if (ShouldEnableBootstrapSandbox() &&
          bootstrap_sandbox_policy != SANDBOX_TYPE_INVALID) {
        options.replacement_bootstrap_name =
            GetBootstrapSandbox()->server_bootstrap_name();
        GetBootstrapSandbox()->PrepareToForkWithPolicy(
            bootstrap_sandbox_policy);
      }
#endif  // defined(OS_MACOSX)

      bool launched = base::LaunchProcess(*cmd_line, options, &handle);
      if (!launched)
        handle = base::kNullProcessHandle;

#if defined(OS_MACOSX)
      if (ShouldEnableBootstrapSandbox() &&
          bootstrap_sandbox_policy != SANDBOX_TYPE_INVALID) {
        GetBootstrapSandbox()->FinishedFork(handle);
      }

      if (launched)
        broker->AddPlaceholderForPid(handle);

      // After updating the broker, release the lock and let the child's
      // messasge be processed on the broker's thread.
      broker->GetLock().Release();
#endif  // defined(OS_MACOSX)
    }
#endif  // else defined(OS_POSIX)
#if !defined(OS_ANDROID)
  if (handle)
    RecordHistograms(begin_launch_time);
  BrowserThread::PostTask(
      client_thread_id, FROM_HERE,
      base::Bind(
          &Context::Notify,
          this_object.get(),
#if defined(OS_POSIX) && !defined(OS_MACOSX)
          use_zygote,
#endif
          handle));
#endif  // !defined(OS_ANDROID)
  }

  void Notify(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::ProcessHandle handle) {
#if defined(OS_ANDROID)
    // Finally close the ipcfd
    base::ScopedFD ipcfd_closer(ipcfd_);
#endif
    starting_ = false;
    process_.set_handle(handle);
    if (!handle)
      LOG(ERROR) << "Failed to launch child process";

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    zygote_ = zygote;
#endif
    if (client_) {
      if (handle) {
        client_->OnProcessLaunched();
      } else {
        client_->OnProcessLaunchFailed();
      }
    } else {
      Terminate();
    }
  }

  void Terminate() {
    if (!process_.handle())
      return;

    if (!terminate_child_on_shutdown_)
      return;

    // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
    // don't this on the UI/IO threads.
    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(
            &Context::TerminateInternal,
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
            zygote_,
#endif
            process_.handle()));
    process_.set_handle(base::kNullProcessHandle);
  }

  static void SetProcessBackgrounded(base::ProcessHandle handle,
                                     bool background) {
    base::Process process(handle);
    process.SetProcessBackgrounded(background);
#if defined(OS_ANDROID)
    SetChildProcessInForeground(handle, !background);
#endif
  }

  static void TerminateInternal(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::ProcessHandle handle) {
#if defined(OS_ANDROID)
    VLOG(0) << "ChromeProcess: Stopping process with handle " << handle;
    StopChildProcess(handle);
#else
    base::Process process(handle);
     // Client has gone away, so just kill the process.  Using exit code 0
    // means that UMA won't treat this as a crash.
    process.Terminate(RESULT_CODE_NORMAL_EXIT);
    // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
#if !defined(OS_MACOSX)
    if (zygote) {
      // If the renderer was created via a zygote, we have to proxy the reaping
      // through the zygote process.
      ZygoteHostImpl::GetInstance()->EnsureProcessTerminated(handle);
    } else
#endif  // !OS_MACOSX
    {
      base::EnsureProcessTerminated(handle);
    }
#endif  // OS_POSIX
    process.Close();
#endif  // defined(OS_ANDROID)
  }

  Client* client_;
  BrowserThread::ID client_thread_id_;
  base::Process process_;
  base::TerminationStatus termination_status_;
  int exit_code_;
  bool starting_;
  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  bool terminate_child_on_shutdown_;
#if defined(OS_ANDROID)
  // The fd to close after creating the process.
  int ipcfd_;
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  bool zygote_;
#endif
};


ChildProcessLauncher::ChildProcessLauncher(
    SandboxedProcessLauncherDelegate* delegate,
    CommandLine* cmd_line,
    int child_process_id,
    Client* client) {
  context_ = new Context();
  context_->Launch(
      delegate,
      cmd_line,
      child_process_id,
      client);
}

ChildProcessLauncher::~ChildProcessLauncher() {
  context_->ResetClient();
}

bool ChildProcessLauncher::IsStarting() {
  return context_->starting_;
}

base::ProcessHandle ChildProcessLauncher::GetHandle() {
  DCHECK(!context_->starting_);
  return context_->process_.handle();
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    bool known_dead,
    int* exit_code) {
  base::ProcessHandle handle = context_->process_.handle();
  if (handle == base::kNullProcessHandle) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = context_->exit_code_;
    return context_->termination_status_;
  }
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  if (context_->zygote_) {
    context_->termination_status_ = ZygoteHostImpl::GetInstance()->
        GetTerminationStatus(handle, known_dead, &context_->exit_code_);
  } else if (known_dead) {
    context_->termination_status_ =
        base::GetKnownDeadTerminationStatus(handle, &context_->exit_code_);
  } else {
#elif defined(OS_MACOSX)
  if (known_dead) {
    context_->termination_status_ =
        base::GetKnownDeadTerminationStatus(handle, &context_->exit_code_);
  } else {
#elif defined(OS_ANDROID)
  if (IsChildProcessOomProtected(handle)) {
      context_->termination_status_ = base::TERMINATION_STATUS_OOM_PROTECTED;
  } else {
#else
  {
#endif
    context_->termination_status_ =
        base::GetTerminationStatus(handle, &context_->exit_code_);
  }

  if (exit_code)
    *exit_code = context_->exit_code_;

  // POSIX: If the process crashed, then the kernel closed the socket
  // for it and so the child has already died by the time we get
  // here. Since GetTerminationStatus called waitpid with WNOHANG,
  // it'll reap the process.  However, if GetTerminationStatus didn't
  // reap the child (because it was still running), we'll need to
  // Terminate via ProcessWatcher. So we can't close the handle here.
  if (context_->termination_status_ != base::TERMINATION_STATUS_STILL_RUNNING)
    context_->process_.Close();

  return context_->termination_status_;
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  BrowserThread::PostTask(
     BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
     base::Bind(
         &ChildProcessLauncher::Context::SetProcessBackgrounded,
         GetHandle(), background));
}

void ChildProcessLauncher::SetTerminateChildOnShutdown(
    bool terminate_on_shutdown) {
  if (context_.get())
    context_->set_terminate_child_on_shutdown(terminate_on_shutdown);
}

}  // namespace content
