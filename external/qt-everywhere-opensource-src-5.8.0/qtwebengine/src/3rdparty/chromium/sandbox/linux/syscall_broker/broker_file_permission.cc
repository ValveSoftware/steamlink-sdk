// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_file_permission.h"

#include <fcntl.h>
#include <stddef.h>
#include <string.h>

#include <string>

#include "base/logging.h"
#include "sandbox/linux/syscall_broker/broker_common.h"

namespace sandbox {

namespace syscall_broker {

// Async signal safe
bool BrokerFilePermission::ValidatePath(const char* path) {
  if (!path)
    return false;

  const size_t len = strlen(path);
  // No empty paths
  if (len == 0)
    return false;
  // Paths must be absolute and not relative
  if (path[0] != '/')
    return false;
  // No trailing / (but "/" is valid)
  if (len > 1 && path[len - 1] == '/')
    return false;
  // No trailing /..
  if (len >= 3 && path[len - 3] == '/' && path[len - 2] == '.' &&
      path[len - 1] == '.')
    return false;
  // No /../ anywhere
  for (size_t i = 0; i < len; i++) {
    if (path[i] == '/' && (len - i) > 3) {
      if (path[i + 1] == '.' && path[i + 2] == '.' && path[i + 3] == '/') {
        return false;
      }
    }
  }
  return true;
}

// Async signal safe
// Calls std::string::c_str(), strncmp and strlen. All these
// methods are async signal safe in common standard libs.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::MatchPath(const char* requested_filename) const {
  const char* path = path_.c_str();
  if ((recursive_ && strncmp(requested_filename, path, strlen(path)) == 0)) {
    // Note: This prefix match will allow any path under the whitelisted
    // path, for any number of directory levels. E.g. if the whitelisted
    // path is /good/ then the following will be permitted by the policy.
    //   /good/file1
    //   /good/folder/file2
    //   /good/folder/folder2/file3
    // If an attacker could make 'folder' a symlink to ../../ they would have
    // access to the entire filesystem.
    // Whitelisting with multiple depths is useful, e.g /proc/ but
    // the system needs to ensure symlinks can not be created!
    // That said if an attacker can convert any of the absolute paths
    // to a symlink they can control any file on the system also.
    return true;
  } else if (strcmp(requested_filename, path) == 0) {
    return true;
  }
  return false;
}

// Async signal safe.
// External call to std::string::c_str() is
// called in MatchPath.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::CheckAccess(const char* requested_filename,
                                       int mode,
                                       const char** file_to_access) const {
  // First, check if |mode| is existence, ability to read or ability
  // to write. We do not support X_OK.
  if (mode != F_OK && mode & ~(R_OK | W_OK)) {
    return false;
  }

  if (!ValidatePath(requested_filename))
    return false;

  if (!MatchPath(requested_filename)) {
    return false;
  }
  bool allowed = false;
  switch (mode) {
    case F_OK:
      if (allow_read_ || allow_write_)
        allowed = true;
      break;
    case R_OK:
      if (allow_read_)
        allowed = true;
      break;
    case W_OK:
      if (allow_write_)
        allowed = true;
      break;
    case R_OK | W_OK:
      if (allow_read_ && allow_write_)
        allowed = true;
      break;
    default:
      return false;
  }

  if (allowed && file_to_access) {
    if (!recursive_)
      *file_to_access = path_.c_str();
    else
      *file_to_access = requested_filename;
  }
  return allowed;
}

// Async signal safe.
// External call to std::string::c_str() is
// called in MatchPath.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::CheckOpen(const char* requested_filename,
                                     int flags,
                                     const char** file_to_open,
                                     bool* unlink_after_open) const {
  if (!ValidatePath(requested_filename))
    return false;

  if (!MatchPath(requested_filename)) {
    return false;
  }

  // First, check the access mode is valid.
  const int access_mode = flags & O_ACCMODE;
  if (access_mode != O_RDONLY && access_mode != O_WRONLY &&
      access_mode != O_RDWR) {
    return false;
  }

  // Check if read is allowed
  if (!allow_read_ && (access_mode == O_RDONLY || access_mode == O_RDWR)) {
    return false;
  }

  // Check if write is allowed
  if (!allow_write_ && (access_mode == O_WRONLY || access_mode == O_RDWR)) {
    return false;
  }

  // Check if file creation is allowed.
  if (!allow_create_ && (flags & O_CREAT)) {
    return false;
  }

  // If O_CREAT is present, ensure O_EXCL
  if ((flags & O_CREAT) && !(flags & O_EXCL)) {
    return false;
  }

  // If this file is to be unlinked, ensure it's created.
  if (unlink_ && !(flags & O_CREAT)) {
    return false;
  }

  // Some flags affect the behavior of the current process. We don't support
  // them and don't allow them for now.
  if (flags & kCurrentProcessOpenFlagsMask) {
    return false;
  }

  // Now check that all the flags are known to us.
  const int creation_and_status_flags = flags & ~O_ACCMODE;

  const int known_flags = O_APPEND | O_ASYNC | O_CLOEXEC | O_CREAT | O_DIRECT |
                          O_DIRECTORY | O_EXCL | O_LARGEFILE | O_NOATIME |
                          O_NOCTTY | O_NOFOLLOW | O_NONBLOCK | O_NDELAY |
                          O_SYNC | O_TRUNC;

  const int unknown_flags = ~known_flags;
  const bool has_unknown_flags = creation_and_status_flags & unknown_flags;

  if (has_unknown_flags)
    return false;

  if (file_to_open) {
    if (!recursive_)
      *file_to_open = path_.c_str();
    else
      *file_to_open = requested_filename;
  }
  if (unlink_after_open)
    *unlink_after_open = unlink_;

  return true;
}

const char* BrokerFilePermission::GetErrorMessageForTests() {
  static char kInvalidBrokerFileString[] = "Invalid BrokerFilePermission";
  return kInvalidBrokerFileString;
}

BrokerFilePermission::BrokerFilePermission(const std::string& path,
                                           bool recursive,
                                           bool unlink,
                                           bool allow_read,
                                           bool allow_write,
                                           bool allow_create)
    : path_(path),
      recursive_(recursive),
      unlink_(unlink),
      allow_read_(allow_read),
      allow_write_(allow_write),
      allow_create_(allow_create) {
  // Validate this permission and die if invalid!

  // Must have enough length for a '/'
  CHECK(path_.length() > 0) << GetErrorMessageForTests();
  // Whitelisted paths must be absolute.
  CHECK(path_[0] == '/') << GetErrorMessageForTests();

  // Don't allow unlinking on creation without create permission
  if (unlink_) {
    CHECK(allow_create) << GetErrorMessageForTests();
  }
  const char last_char = *(path_.rbegin());
  // Recursive paths must have a trailing slash
  if (recursive_) {
    CHECK(last_char == '/') << GetErrorMessageForTests();
  } else {
    CHECK(last_char != '/') << GetErrorMessageForTests();
  }
}

}  // namespace syscall_broker

}  // namespace sandbox