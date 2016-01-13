// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is duplicated from base/linux_utils.cc.
// We shouldn't link against C++ code in a setuid binary.

// Needed for O_DIRECTORY, must be defined before fcntl.h is included
// (and it can be included earlier than the explicit #include below
// in some versions of glibc).
#define _GNU_SOURCE

#include "sandbox/linux/suid/linux_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// expected prefix of the target of the /proc/self/fd/%d link for a socket
static const char kSocketLinkPrefix[] = "socket:[";

// Parse a symlink in /proc/pid/fd/$x and return the inode number of the
// socket.
//   inode_out: (output) set to the inode number on success
//   path: e.g. /proc/1234/fd/5 (must be a UNIX domain socket descriptor)
static bool ProcPathGetInodeAt(ino_t* inode_out,
                               int base_dir_fd,
                               const char* path) {
  // We also check that the path is relative.
  if (!inode_out || !path || *path == '/')
    return false;
  char buf[256];
  const ssize_t n = readlinkat(base_dir_fd, path, buf, sizeof(buf) - 1);
  if (n < 0)
    return false;
  buf[n] = 0;

  if (memcmp(kSocketLinkPrefix, buf, sizeof(kSocketLinkPrefix) - 1))
    return false;

  char* endptr = NULL;
  errno = 0;
  const unsigned long long int inode_ull =
      strtoull(buf + sizeof(kSocketLinkPrefix) - 1, &endptr, 10);
  if (inode_ull == ULLONG_MAX || !endptr || *endptr != ']' || errno != 0)
    return false;

  *inode_out = inode_ull;
  return true;
}

static DIR* opendirat(int base_dir_fd, const char* name) {
  // Also check that |name| is relative.
  if (base_dir_fd < 0 || !name || *name == '/')
    return NULL;
  int new_dir_fd = openat(base_dir_fd, name, O_RDONLY | O_DIRECTORY);
  if (new_dir_fd < 0)
    return NULL;

  return fdopendir(new_dir_fd);
}

bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode) {
  bool already_found = false;

  DIR* proc = opendir("/proc");
  if (!proc)
    return false;

  const uid_t uid = getuid();
  struct dirent* dent;
  while ((dent = readdir(proc))) {
    char* endptr = NULL;
    errno = 0;
    const unsigned long int pid_ul = strtoul(dent->d_name, &endptr, 10);
    if (pid_ul == ULONG_MAX || !endptr || *endptr || errno != 0)
      continue;

    // We have this setuid code here because the zygote and its children have
    // /proc/$pid/fd owned by root. While scanning through /proc, we add this
    // extra check so users cannot accidentally gain information about other
    // users' processes. To determine process ownership, we use the property
    // that if user foo owns process N, then /proc/N is owned by foo.
    int proc_pid_fd = -1;
    {
      char buf[256];
      struct stat statbuf;
      snprintf(buf, sizeof(buf), "/proc/%lu", pid_ul);
      proc_pid_fd = open(buf, O_RDONLY | O_DIRECTORY);
      if (proc_pid_fd < 0)
        continue;
      if (fstat(proc_pid_fd, &statbuf) < 0 || uid != statbuf.st_uid) {
        close(proc_pid_fd);
        continue;
      }
    }

    DIR* fd = opendirat(proc_pid_fd, "fd");
    if (!fd) {
      close(proc_pid_fd);
      continue;
    }

    while ((dent = readdir(fd))) {
      char buf[256];
      int printed = snprintf(buf, sizeof(buf), "fd/%s", dent->d_name);
      if (printed < 0 || printed >= (int)(sizeof(buf) - 1)) {
        continue;
      }

      ino_t fd_inode;
      if (ProcPathGetInodeAt(&fd_inode, proc_pid_fd, buf)) {
        if (fd_inode == socket_inode) {
          if (already_found) {
            closedir(fd);
            close(proc_pid_fd);
            closedir(proc);
            return false;
          }

          already_found = true;
          *pid_out = pid_ul;
          break;
        }
      }
    }
    closedir(fd);
    close(proc_pid_fd);
  }
  closedir(proc);

  return already_found;
}
