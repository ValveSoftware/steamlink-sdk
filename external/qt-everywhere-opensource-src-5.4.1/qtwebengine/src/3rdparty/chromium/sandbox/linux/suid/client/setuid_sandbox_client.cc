// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "sandbox/linux/suid/common/suid_unsafe_environment_variables.h"

namespace {

bool IsFileSystemAccessDenied() {
  base::ScopedFD self_exe(HANDLE_EINTR(open(base::kProcSelfExe, O_RDONLY)));
  return !self_exe.is_valid();
}

// Set an environment variable that reflects the API version we expect from the
// setuid sandbox. Old versions of the sandbox will ignore this.
void SetSandboxAPIEnvironmentVariable(base::Environment* env) {
  env->SetVar(sandbox::kSandboxEnvironmentApiRequest,
              base::IntToString(sandbox::kSUIDSandboxApiNumber));
}

// Unset environment variables that are expected to be set by the setuid
// sandbox. This is to allow nesting of one instance of the SUID sandbox
// inside another.
void UnsetExpectedEnvironmentVariables(base::EnvironmentMap* env_map) {
  DCHECK(env_map);
  const base::NativeEnvironmentString environment_vars[] = {
      sandbox::kSandboxDescriptorEnvironmentVarName,
      sandbox::kSandboxHelperPidEnvironmentVarName,
      sandbox::kSandboxEnvironmentApiProvides,
      sandbox::kSandboxPIDNSEnvironmentVarName,
      sandbox::kSandboxNETNSEnvironmentVarName,
  };

  for (size_t i = 0; i < arraysize(environment_vars); ++i) {
    // Setting values in EnvironmentMap to an empty-string will make
    // sure that they get unset from the environment via AlterEnvironment().
    (*env_map)[environment_vars[i]] = base::NativeEnvironmentString();
  }
}

// Wrapper around a shared C function.
// Returns the "saved" environment variable name corresponding to |envvar|
// in a new string or NULL.
std::string* CreateSavedVariableName(const char* env_var) {
  char* const saved_env_var = SandboxSavedEnvironmentVariable(env_var);
  if (!saved_env_var)
    return NULL;
  std::string* saved_env_var_copy = new std::string(saved_env_var);
  // SandboxSavedEnvironmentVariable is the C function that we wrap and uses
  // malloc() to allocate memory.
  free(saved_env_var);
  return saved_env_var_copy;
}

// The ELF loader will clear many environment variables so we save them to
// different names here so that the SUID sandbox can resolve them for the
// renderer.
void SaveSUIDUnsafeEnvironmentVariables(base::Environment* env) {
  for (unsigned i = 0; kSUIDUnsafeEnvironmentVariables[i]; ++i) {
    const char* env_var = kSUIDUnsafeEnvironmentVariables[i];
    // Get the saved environment variable corresponding to envvar.
    scoped_ptr<std::string> saved_env_var(CreateSavedVariableName(env_var));
    if (saved_env_var == NULL)
      continue;

    std::string value;
    if (env->GetVar(env_var, &value))
      env->SetVar(saved_env_var->c_str(), value);
    else
      env->UnSetVar(saved_env_var->c_str());
  }
}

int GetHelperApi(base::Environment* env) {
  std::string api_string;
  int api_number = 0;  // Assume API version 0 if no environment was found.
  if (env->GetVar(sandbox::kSandboxEnvironmentApiProvides, &api_string) &&
      !base::StringToInt(api_string, &api_number)) {
    // It's an error if we could not convert the API number.
    api_number = -1;
  }
  return api_number;
}

// Convert |var_name| from the environment |env| to an int.
// Return -1 if the variable does not exist or the value cannot be converted.
int EnvToInt(base::Environment* env, const char* var_name) {
  std::string var_string;
  int var_value = -1;
  if (env->GetVar(var_name, &var_string) &&
      !base::StringToInt(var_string, &var_value)) {
    var_value = -1;
  }
  return var_value;
}

pid_t GetHelperPID(base::Environment* env) {
  return EnvToInt(env, sandbox::kSandboxHelperPidEnvironmentVarName);
}

// Get the IPC file descriptor used to communicate with the setuid helper.
int GetIPCDescriptor(base::Environment* env) {
  return EnvToInt(env, sandbox::kSandboxDescriptorEnvironmentVarName);
}

const char* GetDevelSandboxPath() {
  return getenv("CHROME_DEVEL_SANDBOX");
}

}  // namespace

namespace sandbox {

SetuidSandboxClient* SetuidSandboxClient::Create() {
  base::Environment* environment(base::Environment::Create());
  SetuidSandboxClient* sandbox_client(new SetuidSandboxClient);

  CHECK(environment);
  sandbox_client->env_ = environment;
  return sandbox_client;
}

SetuidSandboxClient::SetuidSandboxClient()
    : env_(NULL),
      sandboxed_(false) {
}

SetuidSandboxClient::~SetuidSandboxClient() {
  delete env_;
}

void SetuidSandboxClient::CloseDummyFile() {
  // When we're launched through the setuid sandbox, SetupLaunchOptions
  // arranges for kZygoteIdFd to be a dummy file descriptor to satisfy an
  // ancient setuid sandbox ABI requirement. However, the descriptor is no
  // longer needed, so we can simply close it right away now.
  CHECK(IsSuidSandboxChild());

  // Sanity check that kZygoteIdFd refers to a pipe.
  struct stat st;
  PCHECK(0 == fstat(kZygoteIdFd, &st));
  CHECK(S_ISFIFO(st.st_mode));

  PCHECK(0 == IGNORE_EINTR(close(kZygoteIdFd)));
}

bool SetuidSandboxClient::ChrootMe() {
  int ipc_fd = GetIPCDescriptor(env_);

  if (ipc_fd < 0) {
    LOG(ERROR) << "Failed to obtain the sandbox IPC descriptor";
    return false;
  }

  if (HANDLE_EINTR(write(ipc_fd, &kMsgChrootMe, 1)) != 1) {
    PLOG(ERROR) << "Failed to write to chroot pipe";
    return false;
  }

  // We need to reap the chroot helper process in any event.
  pid_t helper_pid = GetHelperPID(env_);
  // If helper_pid is -1 we wait for any child.
  if (HANDLE_EINTR(waitpid(helper_pid, NULL, 0)) < 0) {
    PLOG(ERROR) << "Failed to wait for setuid helper to die";
    return false;
  }

  char reply;
  if (HANDLE_EINTR(read(ipc_fd, &reply, 1)) != 1) {
    PLOG(ERROR) << "Failed to read from chroot pipe";
    return false;
  }

  if (reply != kMsgChrootSuccessful) {
    LOG(ERROR) << "Error code reply from chroot helper";
    return false;
  }

  // We now consider ourselves "fully sandboxed" as far as the
  // setuid sandbox is concerned.
  CHECK(IsFileSystemAccessDenied());
  sandboxed_ = true;
  return true;
}

bool SetuidSandboxClient::CreateInitProcessReaper(
    base::Closure* post_fork_parent_callback) {
  return sandbox::CreateInitProcessReaper(post_fork_parent_callback);
}

bool SetuidSandboxClient::IsSuidSandboxUpToDate() const {
  return GetHelperApi(env_) == kSUIDSandboxApiNumber;
}

bool SetuidSandboxClient::IsSuidSandboxChild() const {
  return GetIPCDescriptor(env_) >= 0;
}

bool SetuidSandboxClient::IsInNewPIDNamespace() const {
  return env_->HasVar(kSandboxPIDNSEnvironmentVarName);
}

bool SetuidSandboxClient::IsInNewNETNamespace() const {
  return env_->HasVar(kSandboxNETNSEnvironmentVarName);
}

bool SetuidSandboxClient::IsSandboxed() const {
  return sandboxed_;
}

// Check if CHROME_DEVEL_SANDBOX is set but empty. This currently disables
// the setuid sandbox. TODO(jln): fix this (crbug.com/245376).
bool SetuidSandboxClient::IsDisabledViaEnvironment() {
  const char* devel_sandbox_path = GetDevelSandboxPath();
  if (devel_sandbox_path && '\0' == *devel_sandbox_path) {
    return true;
  }
  return false;
}

base::FilePath SetuidSandboxClient::GetSandboxBinaryPath() {
  base::FilePath sandbox_binary;
  base::FilePath exe_dir;
  if (PathService::Get(base::DIR_EXE, &exe_dir)) {
    base::FilePath sandbox_candidate = exe_dir.AppendASCII("chrome-sandbox");
    if (base::PathExists(sandbox_candidate))
      sandbox_binary = sandbox_candidate;
  }

  // In user-managed builds, including development builds, an environment
  // variable is required to enable the sandbox. See
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
  struct stat st;
  if (sandbox_binary.empty() && stat(base::kProcSelfExe, &st) == 0 &&
      st.st_uid == getuid()) {
    const char* devel_sandbox_path = GetDevelSandboxPath();
    if (devel_sandbox_path) {
      sandbox_binary = base::FilePath(devel_sandbox_path);
    }
  }

  return sandbox_binary;
}

void SetuidSandboxClient::PrependWrapper(base::CommandLine* cmd_line) {
  std::string sandbox_binary(GetSandboxBinaryPath().value());
  struct stat st;
  if (sandbox_binary.empty() || stat(sandbox_binary.c_str(), &st) != 0) {
    LOG(FATAL) << "The SUID sandbox helper binary is missing: "
               << sandbox_binary << " Aborting now. See "
                                    "https://code.google.com/p/chromium/wiki/"
                                    "LinuxSUIDSandboxDevelopment.";
  }

  if (access(sandbox_binary.c_str(), X_OK) != 0 || (st.st_uid != 0) ||
      ((st.st_mode & S_ISUID) == 0) || ((st.st_mode & S_IXOTH)) == 0) {
    LOG(FATAL) << "The SUID sandbox helper binary was found, but is not "
                  "configured correctly. Rather than run without sandboxing "
                  "I'm aborting now. You need to make sure that "
               << sandbox_binary << " is owned by root and has mode 4755.";
  }

  cmd_line->PrependWrapper(sandbox_binary);
}

void SetuidSandboxClient::SetupLaunchOptions(
    base::LaunchOptions* options,
    base::FileHandleMappingVector* fds_to_remap,
    base::ScopedFD* dummy_fd) {
  DCHECK(options);
  DCHECK(fds_to_remap);

  // Launching a setuid binary requires PR_SET_NO_NEW_PRIVS to not be used.
  options->allow_new_privs = true;
  UnsetExpectedEnvironmentVariables(&options->environ);

  // Set dummy_fd to the reading end of a closed pipe.
  int pipe_fds[2];
  PCHECK(0 == pipe(pipe_fds));
  PCHECK(0 == IGNORE_EINTR(close(pipe_fds[1])));
  dummy_fd->reset(pipe_fds[0]);

  // We no longer need a dummy socket for discovering the child's PID,
  // but the sandbox is still hard-coded to expect a file descriptor at
  // kZygoteIdFd. Fixing this requires a sandbox API change. :(
  fds_to_remap->push_back(std::make_pair(dummy_fd->get(), kZygoteIdFd));
}

void SetuidSandboxClient::SetupLaunchEnvironment() {
  SaveSUIDUnsafeEnvironmentVariables(env_);
  SetSandboxAPIEnvironmentVariable(env_);
}

}  // namespace sandbox
