// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/zygote/zygote_linux.h"

#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/kill.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/set_process_title.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#endif

// See http://code.google.com/p/chromium/wiki/LinuxZygote

namespace content {

namespace {

// NOP function. See below where this handler is installed.
void SIGCHLDHandler(int signal) {
}

int LookUpFd(const base::GlobalDescriptors::Mapping& fd_mapping, uint32_t key) {
  for (size_t index = 0; index < fd_mapping.size(); ++index) {
    if (fd_mapping[index].first == key)
      return fd_mapping[index].second;
  }
  return -1;
}

void CreatePipe(base::ScopedFD* read_pipe, base::ScopedFD* write_pipe) {
  int raw_pipe[2];
  PCHECK(0 == pipe(raw_pipe));
  read_pipe->reset(raw_pipe[0]);
  write_pipe->reset(raw_pipe[1]);
}

void KillAndReap(pid_t pid, ZygoteForkDelegate* helper) {
  if (helper) {
    // Helper children may be forked in another PID namespace, so |pid| might
    // be meaningless to us; or we just might not be able to directly send it
    // signals.  So we can't kill it.
    // Additionally, we're not its parent, so we can't reap it anyway.
    // TODO(mdempsky): Extend the ZygoteForkDelegate API to handle this.
    LOG(WARNING) << "Unable to kill or reap helper children";
    return;
  }

  // Kill the child process in case it's not already dead, so we can safely
  // perform a blocking wait.
  PCHECK(0 == kill(pid, SIGKILL));
  PCHECK(pid == HANDLE_EINTR(waitpid(pid, NULL, 0)));
}

}  // namespace

Zygote::Zygote(int sandbox_flags, ScopedVector<ZygoteForkDelegate> helpers,
               const std::vector<base::ProcessHandle>& extra_children,
               const std::vector<int>& extra_fds)
    : sandbox_flags_(sandbox_flags),
      helpers_(helpers.Pass()),
      initial_uma_index_(0),
      extra_children_(extra_children),
      extra_fds_(extra_fds) {}

Zygote::~Zygote() {
}

bool Zygote::ProcessRequests() {
  // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
  // browser on it.
  // A SOCK_DGRAM is installed in fd 5. This is the sandbox IPC channel.
  // See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = &SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  if (UsingSUIDSandbox()) {
    // Let the ZygoteHost know we are ready to go.
    // The receiving code is in content/browser/zygote_host_linux.cc.
    bool r = UnixDomainSocket::SendMsg(kZygoteSocketPairFd,
                                       kZygoteHelloMessage,
                                       sizeof(kZygoteHelloMessage),
                                       std::vector<int>());
#if defined(OS_CHROMEOS)
    LOG_IF(WARNING, !r) << "Sending zygote magic failed";
    // Exit normally on chromeos because session manager may send SIGTERM
    // right after the process starts and it may fail to send zygote magic
    // number to browser process.
    if (!r)
      _exit(RESULT_CODE_NORMAL_EXIT);
#else
    CHECK(r) << "Sending zygote magic failed";
#endif
  }

  for (;;) {
    // This function call can return multiple times, once per fork().
    if (HandleRequestFromBrowser(kZygoteSocketPairFd))
      return true;
  }
}

bool Zygote::GetProcessInfo(base::ProcessHandle pid,
                            ZygoteProcessInfo* process_info) {
  DCHECK(process_info);
  const ZygoteProcessMap::const_iterator it = process_info_map_.find(pid);
  if (it == process_info_map_.end()) {
    return false;
  }
  *process_info = it->second;
  return true;
}

bool Zygote::UsingSUIDSandbox() const {
  return sandbox_flags_ & kSandboxLinuxSUID;
}

bool Zygote::HandleRequestFromBrowser(int fd) {
  ScopedVector<base::ScopedFD> fds;
  char buf[kZygoteMaxMessageLength];
  const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);

  if (len == 0 || (len == -1 && errno == ECONNRESET)) {
    // EOF from the browser. We should die.
    // TODO(earthdok): call __sanititizer_cov_dump() here to obtain code
    // coverage  for the Zygote. Currently it's not possible because of
    // confusion over who is responsible for closing the file descriptor.
    for (std::vector<int>::iterator it = extra_fds_.begin();
         it < extra_fds_.end(); ++it) {
      PCHECK(0 == IGNORE_EINTR(close(*it)));
    }
#if !defined(ADDRESS_SANITIZER)
    // TODO(earthdok): add watchdog thread before using this in non-ASAN builds.
    CHECK(extra_children_.empty());
#endif
    for (std::vector<base::ProcessHandle>::iterator it =
             extra_children_.begin();
         it < extra_children_.end(); ++it) {
      PCHECK(*it == HANDLE_EINTR(waitpid(*it, NULL, 0)));
    }
    _exit(0);
    return false;
  }

  if (len == -1) {
    PLOG(ERROR) << "Error reading message from browser";
    return false;
  }

  Pickle pickle(buf, len);
  PickleIterator iter(pickle);

  int kind;
  if (pickle.ReadInt(&iter, &kind)) {
    switch (kind) {
      case kZygoteCommandFork:
        // This function call can return multiple times, once per fork().
        return HandleForkRequest(fd, pickle, iter, fds.Pass());

      case kZygoteCommandReap:
        if (!fds.empty())
          break;
        HandleReapRequest(fd, pickle, iter);
        return false;
      case kZygoteCommandGetTerminationStatus:
        if (!fds.empty())
          break;
        HandleGetTerminationStatus(fd, pickle, iter);
        return false;
      case kZygoteCommandGetSandboxStatus:
        HandleGetSandboxStatus(fd, pickle, iter);
        return false;
      case kZygoteCommandForkRealPID:
        // This shouldn't happen in practice, but some failure paths in
        // HandleForkRequest (e.g., if ReadArgsAndFork fails during depickling)
        // could leave this command pending on the socket.
        LOG(ERROR) << "Unexpected real PID message from browser";
        NOTREACHED();
        return false;
      default:
        NOTREACHED();
        break;
    }
  }

  LOG(WARNING) << "Error parsing message from browser";
  return false;
}

// TODO(jln): remove callers to this broken API. See crbug.com/274855.
void Zygote::HandleReapRequest(int fd,
                               const Pickle& pickle,
                               PickleIterator iter) {
  base::ProcessId child;

  if (!pickle.ReadInt(&iter, &child)) {
    LOG(WARNING) << "Error parsing reap request from browser";
    return;
  }

  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(child, &child_info)) {
    LOG(ERROR) << "Child not found!";
    NOTREACHED();
    return;
  }

  if (!child_info.started_from_helper) {
    // Do not call base::EnsureProcessTerminated() under ThreadSanitizer, as it
    // spawns a separate thread which may live until the call to fork() in the
    // zygote. As a result, ThreadSanitizer will report an error and almost
    // disable race detection in the child process.
    // Not calling EnsureProcessTerminated() may result in zombie processes
    // sticking around. This will only happen during testing, so we can live
    // with this for now.
#if !defined(THREAD_SANITIZER)
    // TODO(jln): this old code is completely broken. See crbug.com/274855.
    base::EnsureProcessTerminated(child_info.internal_pid);
#else
    LOG(WARNING) << "Zygote process omitting a call to "
        << "base::EnsureProcessTerminated() for child pid " << child
        << " under ThreadSanitizer. See http://crbug.com/274855.";
#endif
  } else {
    // For processes from the helper, send a GetTerminationStatus request
    // with known_dead set to true.
    // This is not perfect, as the process may be killed instantly, but is
    // better than ignoring the request.
    base::TerminationStatus status;
    int exit_code;
    bool got_termination_status =
        GetTerminationStatus(child, true /* known_dead */, &status, &exit_code);
    DCHECK(got_termination_status);
  }
  process_info_map_.erase(child);
}

bool Zygote::GetTerminationStatus(base::ProcessHandle real_pid,
                                  bool known_dead,
                                  base::TerminationStatus* status,
                                  int* exit_code) {

  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(real_pid, &child_info)) {
    LOG(ERROR) << "Zygote::GetTerminationStatus for unknown PID "
               << real_pid;
    NOTREACHED();
    return false;
  }
  // We know about |real_pid|.
  const base::ProcessHandle child = child_info.internal_pid;
  if (child_info.started_from_helper) {
    if (!child_info.started_from_helper->GetTerminationStatus(
            child, known_dead, status, exit_code)) {
      return false;
    }
  } else {
    // Handle the request directly.
    if (known_dead) {
      *status = base::GetKnownDeadTerminationStatus(child, exit_code);
    } else {
      // We don't know if the process is dying, so get its status but don't
      // wait.
      *status = base::GetTerminationStatus(child, exit_code);
    }
  }
  // Successfully got a status for |real_pid|.
  if (*status != base::TERMINATION_STATUS_STILL_RUNNING) {
    // Time to forget about this process.
    process_info_map_.erase(real_pid);
  }
  return true;
}

void Zygote::HandleGetTerminationStatus(int fd,
                                        const Pickle& pickle,
                                        PickleIterator iter) {
  bool known_dead;
  base::ProcessHandle child_requested;

  if (!pickle.ReadBool(&iter, &known_dead) ||
      !pickle.ReadInt(&iter, &child_requested)) {
    LOG(WARNING) << "Error parsing GetTerminationStatus request "
                 << "from browser";
    return;
  }

  base::TerminationStatus status;
  int exit_code;

  bool got_termination_status =
      GetTerminationStatus(child_requested, known_dead, &status, &exit_code);
  if (!got_termination_status) {
    // Assume that if we can't find the child in the sandbox, then
    // it terminated normally.
    NOTREACHED();
    status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    exit_code = RESULT_CODE_NORMAL_EXIT;
  }

  Pickle write_pickle;
  write_pickle.WriteInt(static_cast<int>(status));
  write_pickle.WriteInt(exit_code);
  ssize_t written =
      HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));
  if (written != static_cast<ssize_t>(write_pickle.size()))
    PLOG(ERROR) << "write";
}

int Zygote::ForkWithRealPid(const std::string& process_type,
                            const base::GlobalDescriptors::Mapping& fd_mapping,
                            const std::string& channel_id,
                            base::ScopedFD pid_oracle,
                            std::string* uma_name,
                            int* uma_sample,
                            int* uma_boundary_value) {
  ZygoteForkDelegate* helper = NULL;
  for (ScopedVector<ZygoteForkDelegate>::iterator i = helpers_.begin();
       i != helpers_.end();
       ++i) {
    if ((*i)->CanHelp(process_type, uma_name, uma_sample, uma_boundary_value)) {
      helper = *i;
      break;
    }
  }

  base::ScopedFD read_pipe, write_pipe;
  base::ProcessId pid = 0;
  if (helper) {
    int ipc_channel_fd = LookUpFd(fd_mapping, kPrimaryIPCChannel);
    if (ipc_channel_fd < 0) {
      DLOG(ERROR) << "Failed to find kPrimaryIPCChannel in FD mapping";
      return -1;
    }
    std::vector<int> fds;
    fds.push_back(ipc_channel_fd);  // kBrowserFDIndex
    fds.push_back(pid_oracle.get());  // kPIDOracleFDIndex
    pid = helper->Fork(process_type, fds, channel_id);

    // Helpers should never return in the child process.
    CHECK_NE(pid, 0);
  } else {
    CreatePipe(&read_pipe, &write_pipe);
    pid = fork();
  }

  if (pid == 0) {
    // In the child process.
    write_pipe.reset();

    // Ping the PID oracle socket so the browser can find our PID.
    CHECK(SendZygoteChildPing(pid_oracle.get()));

    // Now read back our real PID from the zygote.
    base::ProcessId real_pid;
    if (!base::ReadFromFD(read_pipe.get(),
                          reinterpret_cast<char*>(&real_pid),
                          sizeof(real_pid))) {
      LOG(FATAL) << "Failed to synchronise with parent zygote process";
    }
    if (real_pid <= 0) {
      LOG(FATAL) << "Invalid pid from parent zygote";
    }
#if defined(OS_LINUX)
    // Sandboxed processes need to send the global, non-namespaced PID when
    // setting up an IPC channel to their parent.
    IPC::Channel::SetGlobalPid(real_pid);
    // Force the real PID so chrome event data have a PID that corresponds
    // to system trace event data.
    base::debug::TraceLog::GetInstance()->SetProcessID(
        static_cast<int>(real_pid));
#endif
    return 0;
  }

  // In the parent process.
  read_pipe.reset();
  pid_oracle.reset();

  // Always receive a real PID from the zygote host, though it might
  // be invalid (see below).
  base::ProcessId real_pid;
  {
    ScopedVector<base::ScopedFD> recv_fds;
    char buf[kZygoteMaxMessageLength];
    const ssize_t len = UnixDomainSocket::RecvMsg(
        kZygoteSocketPairFd, buf, sizeof(buf), &recv_fds);
    CHECK_GT(len, 0);
    CHECK(recv_fds.empty());

    Pickle pickle(buf, len);
    PickleIterator iter(pickle);

    int kind;
    CHECK(pickle.ReadInt(&iter, &kind));
    CHECK(kind == kZygoteCommandForkRealPID);
    CHECK(pickle.ReadInt(&iter, &real_pid));
  }

  // Fork failed.
  if (pid < 0) {
    return -1;
  }

  // If we successfully forked a child, but it crashed without sending
  // a message to the browser, the browser won't have found its PID.
  if (real_pid < 0) {
    KillAndReap(pid, helper);
    return -1;
  }

  // If we're not using a helper, send the PID back to the child process.
  if (!helper) {
    ssize_t written =
        HANDLE_EINTR(write(write_pipe.get(), &real_pid, sizeof(real_pid)));
    if (written != sizeof(real_pid)) {
      KillAndReap(pid, helper);
      return -1;
    }
  }

  // Now set-up this process to be tracked by the Zygote.
  if (process_info_map_.find(real_pid) != process_info_map_.end()) {
    LOG(ERROR) << "Already tracking PID " << real_pid;
    NOTREACHED();
  }
  process_info_map_[real_pid].internal_pid = pid;
  process_info_map_[real_pid].started_from_helper = helper;

  return real_pid;
}

base::ProcessId Zygote::ReadArgsAndFork(const Pickle& pickle,
                                        PickleIterator iter,
                                        ScopedVector<base::ScopedFD> fds,
                                        std::string* uma_name,
                                        int* uma_sample,
                                        int* uma_boundary_value) {
  std::vector<std::string> args;
  int argc = 0;
  int numfds = 0;
  base::GlobalDescriptors::Mapping mapping;
  std::string process_type;
  std::string channel_id;
  const std::string channel_id_prefix = std::string("--")
      + switches::kProcessChannelID + std::string("=");

  if (!pickle.ReadString(&iter, &process_type))
    return -1;
  if (!pickle.ReadInt(&iter, &argc))
    return -1;

  for (int i = 0; i < argc; ++i) {
    std::string arg;
    if (!pickle.ReadString(&iter, &arg))
      return -1;
    args.push_back(arg);
    if (arg.compare(0, channel_id_prefix.length(), channel_id_prefix) == 0)
      channel_id = arg.substr(channel_id_prefix.length());
  }

  if (!pickle.ReadInt(&iter, &numfds))
    return -1;
  if (numfds != static_cast<int>(fds.size()))
    return -1;

  // First FD is the PID oracle socket.
  if (fds.size() < 1)
    return -1;
  base::ScopedFD pid_oracle(fds[0]->Pass());

  // Remaining FDs are for the global descriptor mapping.
  for (int i = 1; i < numfds; ++i) {
    base::GlobalDescriptors::Key key;
    if (!pickle.ReadUInt32(&iter, &key))
      return -1;
    mapping.push_back(std::make_pair(key, fds[i]->get()));
  }

  mapping.push_back(std::make_pair(
      static_cast<uint32_t>(kSandboxIPCChannel), GetSandboxFD()));

  // Returns twice, once per process.
  base::ProcessId child_pid = ForkWithRealPid(process_type,
                                              mapping,
                                              channel_id,
                                              pid_oracle.Pass(),
                                              uma_name,
                                              uma_sample,
                                              uma_boundary_value);
  if (!child_pid) {
    // This is the child process.

    // Our socket from the browser.
    PCHECK(0 == IGNORE_EINTR(close(kZygoteSocketPairFd)));

    // Pass ownership of file descriptors from fds to GlobalDescriptors.
    for (ScopedVector<base::ScopedFD>::iterator i = fds.begin(); i != fds.end();
         ++i)
      ignore_result((*i)->release());
    base::GlobalDescriptors::GetInstance()->Reset(mapping);

    // Reset the process-wide command line to our new command line.
    CommandLine::Reset();
    CommandLine::Init(0, NULL);
    CommandLine::ForCurrentProcess()->InitFromArgv(args);

    // Update the process title. The argv was already cached by the call to
    // SetProcessTitleFromCommandLine in ChromeMain, so we can pass NULL here
    // (we don't have the original argv at this point).
    SetProcessTitleFromCommandLine(NULL);
  } else if (child_pid < 0) {
    LOG(ERROR) << "Zygote could not fork: process_type " << process_type
        << " numfds " << numfds << " child_pid " << child_pid;
  }
  return child_pid;
}

bool Zygote::HandleForkRequest(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               ScopedVector<base::ScopedFD> fds) {
  std::string uma_name;
  int uma_sample;
  int uma_boundary_value;
  base::ProcessId child_pid = ReadArgsAndFork(
      pickle, iter, fds.Pass(), &uma_name, &uma_sample, &uma_boundary_value);
  if (child_pid == 0)
    return true;
  // If there's no UMA report for this particular fork, then check if any
  // helpers have an initial UMA report for us to send instead.
  while (uma_name.empty() && initial_uma_index_ < helpers_.size()) {
    helpers_[initial_uma_index_++]->InitialUMA(
        &uma_name, &uma_sample, &uma_boundary_value);
  }
  // Must always send reply, as ZygoteHost blocks while waiting for it.
  Pickle reply_pickle;
  reply_pickle.WriteInt(child_pid);
  reply_pickle.WriteString(uma_name);
  if (!uma_name.empty()) {
    reply_pickle.WriteInt(uma_sample);
    reply_pickle.WriteInt(uma_boundary_value);
  }
  if (HANDLE_EINTR(write(fd, reply_pickle.data(), reply_pickle.size())) !=
      static_cast<ssize_t> (reply_pickle.size()))
    PLOG(ERROR) << "write";
  return false;
}

bool Zygote::HandleGetSandboxStatus(int fd,
                                    const Pickle& pickle,
                                    PickleIterator iter) {
  if (HANDLE_EINTR(write(fd, &sandbox_flags_, sizeof(sandbox_flags_))) !=
                   sizeof(sandbox_flags_)) {
    PLOG(ERROR) << "write";
  }

  return false;
}

}  // namespace content
