// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host_impl.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/mojo/constants.h"
#include "content/browser/mojo/mojo_child_connection.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
#include "content/public/browser/zygote_handle_linux.h"
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace content {

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
namespace {
ZygoteHandle g_utility_zygote;
}  // namespace
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

// NOTE: changes to this class need to be reviewed by the security team.
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(const base::FilePath& exposed_dir,
                                          bool launch_elevated,
                                          bool no_sandbox,
                                          const base::EnvironmentMap& env,
                                          ChildProcessHost* host)
      : exposed_dir_(exposed_dir),
#if defined(OS_WIN)
        launch_elevated_(launch_elevated)
#elif defined(OS_POSIX)
        env_(env),
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
        no_sandbox_(no_sandbox),
#endif  // !defined(OS_MACOSX)  && !defined(OS_ANDROID)
        ipc_fd_(host->TakeClientFileDescriptor())
#endif  // OS_WIN
  {}

  ~UtilitySandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool ShouldLaunchElevated() override { return launch_elevated_; }

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (exposed_dir_.empty())
      return true;

    sandbox::ResultCode result;
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_dir_.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    base::FilePath exposed_files = exposed_dir_.AppendASCII("*");
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_files.value().c_str());
    return result == sandbox::SBOX_ALL_OK;
  }

#elif defined(OS_POSIX)

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ZygoteHandle* GetZygote() override {
    if (no_sandbox_ || !exposed_dir_.empty())
      return nullptr;
    return GetGenericZygote();
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)
  base::EnvironmentMap GetEnvironment() override { return env_; }
  base::ScopedFD TakeIpcFd() override { return std::move(ipc_fd_); }
#endif  // OS_WIN

  SandboxType GetSandboxType() override {
    return SANDBOX_TYPE_UTILITY;
  }

 private:
  base::FilePath exposed_dir_;

#if defined(OS_WIN)
  bool launch_elevated_;
#elif defined(OS_POSIX)
  base::EnvironmentMap env_;
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  bool no_sandbox_;
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)
  base::ScopedFD ipc_fd_;
#endif  // OS_WIN
};

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = NULL;

UtilityProcessHost* UtilityProcessHost::Create(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner) {
  return new UtilityProcessHostImpl(client, client_task_runner);
}

void UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHostImpl::UtilityProcessHostImpl(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      is_batch_mode_(false),
      no_sandbox_(false),
      run_elevated_(false),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(base::ASCIIToUTF16("utility process")),
      child_token_(mojo::edk::GenerateRandomToken()),
      weak_ptr_factory_(this) {
  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_UTILITY, this,
                                                 child_token_));
  mojo_child_connection_.reset(new MojoChildConnection(
      kUtilityMojoApplicationName,
      base::StringPrintf("%d_0", process_->GetData().id),
      child_token_,
      MojoShellContext::GetConnectorForIOThread()));
}

UtilityProcessHostImpl::~UtilityProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_batch_mode_)
    EndBatchMode();
}

base::WeakPtr<UtilityProcessHost> UtilityProcessHostImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UtilityProcessHostImpl::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

bool UtilityProcessHostImpl::StartBatchMode()  {
  CHECK(!is_batch_mode_);
  is_batch_mode_ = StartProcess();
  Send(new UtilityMsg_BatchMode_Started());
  return is_batch_mode_;
}

void UtilityProcessHostImpl::EndBatchMode()  {
  CHECK(is_batch_mode_);
  is_batch_mode_ = false;
  Send(new UtilityMsg_BatchMode_Finished());
}

void UtilityProcessHostImpl::SetExposedDir(const base::FilePath& dir) {
  exposed_dir_ = dir;
}

void UtilityProcessHostImpl::DisableSandbox() {
  no_sandbox_ = true;
}

#if defined(OS_WIN)
void UtilityProcessHostImpl::ElevatePrivileges() {
  no_sandbox_ = true;
  run_elevated_ = true;
}
#endif

const ChildProcessData& UtilityProcessHostImpl::GetData() {
  return process_->GetData();
}

#if defined(OS_POSIX)

void UtilityProcessHostImpl::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}

#endif  // OS_POSIX

bool UtilityProcessHostImpl::Start() {
  return StartProcess();
}

shell::InterfaceRegistry* UtilityProcessHostImpl::GetInterfaceRegistry() {
  return mojo_child_connection_->connection()->GetInterfaceRegistry();
}

shell::InterfaceProvider* UtilityProcessHostImpl::GetRemoteInterfaces() {
  if (!mojo_child_connection_->connection()) {
    // During shutdown, connection may be null. We don't care about successfully
    // connecting to remote interfaces at this point, so we just use a dummy
    // provider.
    return &unbound_remote_interfaces_;
  }
  return mojo_child_connection_->connection()->GetRemoteInterfaces();
}

void UtilityProcessHostImpl::SetName(const base::string16& name) {
  name_ = name;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
// static
void UtilityProcessHostImpl::EarlyZygoteLaunch() {
  DCHECK(!g_utility_zygote);
  g_utility_zygote = CreateZygote();
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

bool UtilityProcessHostImpl::StartProcess() {
  if (started_)
    return true;
  started_ = true;

  if (is_batch_mode_)
    return true;

  process_->SetName(name_);

  std::string mojo_channel_token =
      process_->GetHost()->CreateChannelMojo(child_token_);
  if (mojo_channel_token.empty()) {
    NotifyAndDelete(LAUNCH_RESULT_FAILURE);
    return false;
  }

  if (RenderProcessHost::run_renderer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in RenderProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(
        g_utility_main_thread_factory(InProcessChildThreadParams(
            std::string(), BrowserThread::UnsafeGetMessageLoopForThread(
                            BrowserThread::IO)->task_runner(),
            mojo_channel_token, mojo_child_connection_->shell_client_token())));
    in_process_thread_->Start();
  } else {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();

    bool has_cmd_prefix = browser_command_line.HasSwitch(
        switches::kUtilityCmdPrefix);

    #if defined(OS_ANDROID)
      // readlink("/prof/self/exe") sometimes fails on Android at startup.
      // As a workaround skip calling it here, since the executable name is
      // not needed on Android anyway. See crbug.com/500854.
      base::CommandLine* cmd_line =
          new base::CommandLine(base::CommandLine::NO_PROGRAM);
    #else
      int child_flags = child_flags_;

      // When running under gdb, forking /proc/self/exe ends up forking the gdb
      // executable instead of Chromium. It is almost safe to assume that no
      // updates will happen while a developer is running with
      // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
      // a similar case with Valgrind.
      if (has_cmd_prefix)
        child_flags = ChildProcessHost::CHILD_NORMAL;

      base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
      if (exe_path.empty()) {
        NOTREACHED() << "Unable to get utility process binary name.";
        return false;
      }

      base::CommandLine* cmd_line = new base::CommandLine(exe_path);
    #endif

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    cmd_line->AppendSwitchASCII(switches::kMojoChannelToken,
                                mojo_channel_token);
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

#if defined(OS_WIN)
    cmd_line->AppendArg(switches::kPrefetchArgumentOther);
#endif  // defined(OS_WIN)

    if (no_sandbox_)
      cmd_line->AppendSwitch(switches::kNoSandbox);

    // Browser command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
      switches::kNoSandbox,
      switches::kProfilerTiming,
#if defined(OS_MACOSX)
      switches::kEnableSandboxLogging,
#endif
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                               arraysize(kSwitchNames));

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    if (!exposed_dir_.empty()) {
      cmd_line->AppendSwitchPath(switches::kUtilityProcessAllowedDir,
                                 exposed_dir_);
    }

#if defined(OS_WIN)
    // Let the utility process know if it is intended to be elevated.
    if (run_elevated_)
      cmd_line->AppendSwitch(switches::kUtilityProcessRunningElevated);
#endif

    cmd_line->AppendSwitchASCII(switches::kMojoApplicationChannelToken,
                                mojo_child_connection_->shell_client_token());

    process_->Launch(
        new UtilitySandboxedProcessLauncherDelegate(exposed_dir_,
                                                    run_elevated_,
                                                    no_sandbox_, env_,
                                                    process_->GetHost()),
        cmd_line,
        true);
  }

  return true;
}

bool UtilityProcessHostImpl::OnMessageReceived(const IPC::Message& message) {
  if (!client_.get())
    return true;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&UtilityProcessHostClient::OnMessageReceived),
          client_.get(),
          message));

  return true;
}

void UtilityProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UtilityProcessHostClient::OnProcessLaunchFailed,
                 client_.get(),
                 error_code));
}

void UtilityProcessHostImpl::OnProcessCrashed(int exit_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UtilityProcessHostClient::OnProcessCrashed, client_.get(),
            exit_code));
}

void UtilityProcessHostImpl::NotifyAndDelete(int error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&UtilityProcessHostImpl::NotifyLaunchFailedAndDelete,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_code));
}

// static
void UtilityProcessHostImpl::NotifyLaunchFailedAndDelete(
    base::WeakPtr<UtilityProcessHostImpl> host,
    int error_code) {
  if (!host)
    return;

  host->OnProcessLaunchFailed(error_code);
  delete host.get();
}

}  // namespace content
