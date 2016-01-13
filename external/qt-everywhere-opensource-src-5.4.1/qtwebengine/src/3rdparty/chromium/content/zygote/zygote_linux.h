// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_H_
#define CONTENT_ZYGOTE_ZYGOTE_H_

#include <stddef.h>

#include <string>

#include "base/containers/small_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/global_descriptors.h"
#include "base/process/kill.h"
#include "base/process/process.h"

class Pickle;
class PickleIterator;

namespace content {

class ZygoteForkDelegate;

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, simply constructs one of these objects and
// runs it.
class Zygote {
 public:
  Zygote(int sandbox_flags, ScopedVector<ZygoteForkDelegate> helpers,
         const std::vector<base::ProcessHandle>& extra_children,
         const std::vector<int>& extra_fds);
  ~Zygote();

  bool ProcessRequests();

 private:
  struct ZygoteProcessInfo {
    // Pid from inside the Zygote's PID namespace.
    base::ProcessHandle internal_pid;
    // Keeps track of which fork delegate helper the process was started from.
    ZygoteForkDelegate* started_from_helper;
  };
  typedef base::SmallMap< std::map<base::ProcessHandle, ZygoteProcessInfo> >
      ZygoteProcessMap;

  // Retrieve a ZygoteProcessInfo from the process_info_map_.
  // Returns true and write to process_info if |pid| can be found, return
  // false otherwise.
  bool GetProcessInfo(base::ProcessHandle pid,
                      ZygoteProcessInfo* process_info);

  // Returns true if the SUID sandbox is active.
  bool UsingSUIDSandbox() const;

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd);

  void HandleReapRequest(int fd, const Pickle& pickle, PickleIterator iter);

  // Get the termination status of |real_pid|. |real_pid| is the PID as it
  // appears outside of the sandbox.
  // Return true if it managed to get the termination status and return the
  // status in |status| and the exit code in |exit_code|.
  bool GetTerminationStatus(base::ProcessHandle real_pid, bool known_dead,
                            base::TerminationStatus* status,
                            int* exit_code);

  void HandleGetTerminationStatus(int fd,
                                  const Pickle& pickle,
                                  PickleIterator iter);

  // This is equivalent to fork(), except that, when using the SUID sandbox, it
  // returns the real PID of the child process as it appears outside the
  // sandbox, rather than returning the PID inside the sandbox.  The child's
  // real PID is determined by having it call content::SendZygoteChildPing(int)
  // using the |pid_oracle| descriptor.
  // Finally, when using a ZygoteForkDelegate helper, |uma_name|, |uma_sample|,
  // and |uma_boundary_value| may be set if the helper wants to make a UMA
  // report via UMA_HISTOGRAM_ENUMERATION.
  int ForkWithRealPid(const std::string& process_type,
                      const base::GlobalDescriptors::Mapping& fd_mapping,
                      const std::string& channel_id,
                      base::ScopedFD pid_oracle,
                      std::string* uma_name,
                      int* uma_sample,
                      int* uma_boundary_value);

  // Unpacks process type and arguments from |pickle| and forks a new process.
  // Returns -1 on error, otherwise returns twice, returning 0 to the child
  // process and the child process ID to the parent process, like fork().
  base::ProcessId ReadArgsAndFork(const Pickle& pickle,
                                  PickleIterator iter,
                                  ScopedVector<base::ScopedFD> fds,
                                  std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value);

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer. Returns true if we are in a new process,
  // otherwise writes the child_pid back to the browser via |fd|. Writes a
  // child_pid of -1 on error.
  bool HandleForkRequest(int fd,
                         const Pickle& pickle,
                         PickleIterator iter,
                         ScopedVector<base::ScopedFD> fds);

  bool HandleGetSandboxStatus(int fd,
                              const Pickle& pickle,
                              PickleIterator iter);

  // The Zygote needs to keep some information about each process. Most
  // notably what the PID of the process is inside the PID namespace of
  // the Zygote and whether or not a process was started by the
  // ZygoteForkDelegate helper.
  ZygoteProcessMap process_info_map_;

  const int sandbox_flags_;
  ScopedVector<ZygoteForkDelegate> helpers_;

  // Count of how many fork delegates for which we've invoked InitialUMA().
  size_t initial_uma_index_;

  // This vector contains the PIDs of any child processes which have been
  // created prior to the construction of the Zygote object, and must be reaped
  // before the Zygote exits. The Zygote will perform a blocking wait on these
  // children, so they must be guaranteed to be exiting by the time the Zygote
  // exits.
  std::vector<base::ProcessHandle> extra_children_;

  // This vector contains the FDs that must be closed before reaping the extra
  // children.
  std::vector<int> extra_fds_;
};

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_H_
