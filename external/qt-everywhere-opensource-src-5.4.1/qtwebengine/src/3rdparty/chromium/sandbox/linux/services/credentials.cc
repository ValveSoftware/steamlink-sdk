// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/template_util.h"
#include "base/third_party/valgrind/valgrind.h"
#include "base/threading/thread.h"

namespace {

bool IsRunningOnValgrind() { return RUNNING_ON_VALGRIND; }

struct CapFreeDeleter {
  inline void operator()(cap_t cap) const {
    int ret = cap_free(cap);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage libcap2's cap_t type.
typedef scoped_ptr<typeof(*((cap_t)0)), CapFreeDeleter> ScopedCap;

struct CapTextFreeDeleter {
  inline void operator()(char* cap_text) const {
    int ret = cap_free(cap_text);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage the result from libcap2's cap_from_text().
typedef scoped_ptr<char, CapTextFreeDeleter> ScopedCapText;

struct FILECloser {
  inline void operator()(FILE* f) const {
    DCHECK(f);
    PCHECK(0 == fclose(f));
  }
};

// Don't use ScopedFILE in base since it doesn't check fclose().
// TODO(jln): fix base/.
typedef scoped_ptr<FILE, FILECloser> ScopedFILE;

struct DIRCloser {
  void operator()(DIR* d) const {
    DCHECK(d);
    PCHECK(0 == closedir(d));
  }
};

typedef scoped_ptr<DIR, DIRCloser> ScopedDIR;

COMPILE_ASSERT((base::is_same<uid_t, gid_t>::value), UidAndGidAreSameType);
// generic_id_t can be used for either uid_t or gid_t.
typedef uid_t generic_id_t;

// Write a uid or gid mapping from |id| to |id| in |map_file|.
bool WriteToIdMapFile(const char* map_file, generic_id_t id) {
  ScopedFILE f(fopen(map_file, "w"));
  PCHECK(f);
  const uid_t inside_id = id;
  const uid_t outside_id = id;
  int num = fprintf(f.get(), "%d %d 1\n", inside_id, outside_id);
  if (num < 0) return false;
  // Manually call fflush() to catch permission failures.
  int ret = fflush(f.get());
  if (ret) {
    VLOG(1) << "Could not write to id map file";
    return false;
  }
  return true;
}

// Checks that the set of RES-uids and the set of RES-gids have
// one element each and return that element in |resuid| and |resgid|
// respectively. It's ok to pass NULL as one or both of the ids.
bool GetRESIds(uid_t* resuid, gid_t* resgid) {
  uid_t ruid, euid, suid;
  gid_t rgid, egid, sgid;
  PCHECK(getresuid(&ruid, &euid, &suid) == 0);
  PCHECK(getresgid(&rgid, &egid, &sgid) == 0);
  const bool uids_are_equal = (ruid == euid) && (ruid == suid);
  const bool gids_are_equal = (rgid == egid) && (rgid == sgid);
  if (!uids_are_equal || !gids_are_equal) return false;
  if (resuid) *resuid = euid;
  if (resgid) *resgid = egid;
  return true;
}

// chroot() and chdir() to /proc/<tid>/fdinfo.
void ChrootToThreadFdInfo(base::PlatformThreadId tid, bool* result) {
  DCHECK(result);
  *result = false;

  COMPILE_ASSERT((base::is_same<base::PlatformThreadId, int>::value),
                 TidIsAnInt);
  const std::string current_thread_fdinfo = "/proc/" +
      base::IntToString(tid) + "/fdinfo/";

  // Make extra sure that /proc/<tid>/fdinfo is unique to the thread.
  CHECK(0 == unshare(CLONE_FILES));
  int chroot_ret = chroot(current_thread_fdinfo.c_str());
  if (chroot_ret) {
    PLOG(ERROR) << "Could not chroot";
    return;
  }

  // CWD is essentially an implicit file descriptor, so be careful to not leave
  // it behind.
  PCHECK(0 == chdir("/"));

  *result = true;
  return;
}

// chroot() to an empty dir that is "safe". To be safe, it must not contain
// any subdirectory (chroot-ing there would allow a chroot escape) and it must
// be impossible to create an empty directory there.
// We achieve this by doing the following:
// 1. We create a new thread, which will create a new /proc/<tid>/ directory
// 2. We chroot to /proc/<tid>/fdinfo/
// This is already "safe", since fdinfo/ does not contain another directory and
// one cannot create another directory there.
// 3. The thread dies
// After (3) happens, the directory is not available anymore in /proc.
bool ChrootToSafeEmptyDir() {
  base::Thread chrooter("sandbox_chrooter");
  if (!chrooter.Start()) return false;
  bool is_chrooted = false;
  chrooter.message_loop()->PostTask(FROM_HERE,
      base::Bind(&ChrootToThreadFdInfo, chrooter.thread_id(), &is_chrooted));
  // Make sure our task has run before committing the return value.
  chrooter.Stop();
  return is_chrooted;
}

// CHECK() that an attempt to move to a new user namespace raised an expected
// errno.
void CheckCloneNewUserErrno(int error) {
  // EPERM can happen if already in a chroot. EUSERS if too many nested
  // namespaces are used. EINVAL for kernels that don't support the feature.
  // Valgrind will ENOSYS unshare().
  PCHECK(error == EPERM || error == EUSERS || error == EINVAL ||
         error == ENOSYS);
}

}  // namespace.

namespace sandbox {

Credentials::Credentials() {
}

Credentials::~Credentials() {
}

int Credentials::CountOpenFds(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  int proc_self_fd = openat(proc_fd, "self/fd", O_DIRECTORY | O_RDONLY);
  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  int count = 0;
  struct dirent e;
  struct dirent* de;
  while (!readdir_r(dir.get(), &e, &de) && de) {
    if (strcmp(e.d_name, ".") == 0 || strcmp(e.d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(e.d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    ++count;
  }
  return count;
}

bool Credentials::HasOpenDirectory(int proc_fd) {
  int proc_self_fd = -1;
  if (proc_fd >= 0) {
    proc_self_fd = openat(proc_fd, "self/fd", O_DIRECTORY | O_RDONLY);
  } else {
    proc_self_fd = openat(AT_FDCWD, "/proc/self/fd", O_DIRECTORY | O_RDONLY);
    if (proc_self_fd < 0) {
      // If this process has been chrooted (eg into /proc/self/fdinfo) then
      // the new root dir will not have directory listing permissions for us
      // (hence EACCES).  And if we do have this permission, then /proc won't
      // exist anyway (hence ENOENT).
      DPCHECK(errno == EACCES || errno == ENOENT)
        << "Unexpected failure when trying to open /proc/self/fd: ("
        << errno << ") " << strerror(errno);

      // If not available, guess false.
      return false;
    }
  }
  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  struct dirent e;
  struct dirent* de;
  while (!readdir_r(dir.get(), &e, &de) && de) {
    if (strcmp(e.d_name, ".") == 0 || strcmp(e.d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(e.d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    struct stat s;
    // It's OK to use proc_self_fd here, fstatat won't modify it.
    CHECK(fstatat(proc_self_fd, e.d_name, &s, 0) == 0);
    if (S_ISDIR(s.st_mode)) {
      return true;
    }
  }

  // No open unmanaged directories found.
  return false;
}

bool Credentials::DropAllCapabilities() {
  ScopedCap cap(cap_init());
  CHECK(cap);
  PCHECK(0 == cap_set_proc(cap.get()));
  // We never let this function fail.
  return true;
}

bool Credentials::HasAnyCapability() const {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCap empty_cap(cap_init());
  CHECK(empty_cap);
  return cap_compare(current_cap.get(), empty_cap.get()) != 0;
}

scoped_ptr<std::string> Credentials::GetCurrentCapString() const {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCapText cap_text(cap_to_text(current_cap.get(), NULL));
  CHECK(cap_text);
  return scoped_ptr<std::string> (new std::string(cap_text.get()));
}

// static
bool Credentials::SupportsNewUserNS() {
  // Valgrind will let clone(2) pass-through, but doesn't support unshare(),
  // so always consider UserNS unsupported there.
  if (IsRunningOnValgrind()) {
    return false;
  }

  // This is roughly a fork().
  const pid_t pid = syscall(__NR_clone, CLONE_NEWUSER | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    CheckCloneNewUserErrno(errno);
    return false;
  }

  // The parent process could have had threads. In the child, these threads
  // have disappeared. Make sure to not do anything in the child, as this is a
  // fragile execution environment.
  if (pid == 0) {
    _exit(0);
  }

  // Always reap the child.
  siginfo_t infop;
  PCHECK(0 == HANDLE_EINTR(waitid(P_PID, pid, &infop, WEXITED)));

  // clone(2) succeeded, we can use CLONE_NEWUSER.
  return true;
}

bool Credentials::MoveToNewUserNS() {
  uid_t uid;
  gid_t gid;
  if (!GetRESIds(&uid, &gid)) {
    // If all the uids (or gids) are not equal to each other, the security
    // model will most likely confuse the caller, abort.
    DVLOG(1) << "uids or gids differ!";
    return false;
  }
  int ret = unshare(CLONE_NEWUSER);
  if (ret) {
    const int unshare_errno = errno;
    VLOG(1) << "Looks like unprivileged CLONE_NEWUSER may not be available "
            << "on this kernel.";
    CheckCloneNewUserErrno(unshare_errno);
    return false;
  }

  // The current {r,e,s}{u,g}id is now an overflow id (c.f.
  // /proc/sys/kernel/overflowuid). Setup the uid and gid maps.
  DCHECK(GetRESIds(NULL, NULL));
  const char kGidMapFile[] = "/proc/self/gid_map";
  const char kUidMapFile[] = "/proc/self/uid_map";
  CHECK(WriteToIdMapFile(kGidMapFile, gid));
  CHECK(WriteToIdMapFile(kUidMapFile, uid));
  DCHECK(GetRESIds(NULL, NULL));
  return true;
}

bool Credentials::DropFileSystemAccess() {
  // Chrooting to a safe empty dir will only be safe if no directory file
  // descriptor is available to the process.
  DCHECK(!HasOpenDirectory(-1));
  return ChrootToSafeEmptyDir();
}

}  // namespace sandbox.
