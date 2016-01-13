// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include <stdint.h>
#include <stdio.h>  // For |fileno()|.
#include <sys/mman.h>  // For |mmap()|/|munmap()|.
#include <sys/stat.h>
#include <sys/types.h>  // For |off_t|.
#include <unistd.h>

#include <limits>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/embedder/platform_handle.h"

// We assume that |size_t| and |off_t| (type for |ftruncate()|) fits in a
// |uint64_t|.
COMPILE_ASSERT(sizeof(size_t) <= sizeof(uint64_t), size_t_too_big);
COMPILE_ASSERT(sizeof(off_t) <= sizeof(uint64_t), off_t_too_big);

namespace mojo {
namespace system {

// RawSharedBuffer -------------------------------------------------------------

bool RawSharedBuffer::Init() {
  DCHECK(!handle_.is_valid());

  base::ThreadRestrictions::ScopedAllowIO allow_io;

  if (static_cast<uint64_t>(num_bytes_) >
          static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
    return false;
  }

  // TODO(vtl): This is stupid. The implementation of
  // |CreateAndOpenTemporaryFileInDir()| starts with an FD, |fdopen()|s to get a
  // |FILE*|, and then we have to |dup(fileno(fp))| to get back to an FD that we
  // can own. (base/memory/shared_memory_posix.cc does this too, with more
  // |fstat()|s thrown in for good measure.)
  base::FilePath shared_buffer_dir;
  if (!base::GetShmemTempDir(false, &shared_buffer_dir)) {
    LOG(ERROR) << "Failed to get temporary directory for shared memory";
    return false;
  }
  base::FilePath shared_buffer_file;
  base::ScopedFILE fp(base::CreateAndOpenTemporaryFileInDir(
      shared_buffer_dir, &shared_buffer_file));
  if (!fp) {
    LOG(ERROR) << "Failed to create/open temporary file for shared memory";
    return false;
  }
  // Note: |unlink()| is not interruptible.
  if (unlink(shared_buffer_file.value().c_str()) != 0) {
    PLOG(WARNING) << "unlink";
    // This isn't "fatal" (e.g., someone else may have unlinked the file first),
    // so we may as well continue.
  }

  // Note: |dup()| is not interruptible (but |dup2()|/|dup3()| are).
  base::ScopedFD fd(dup(fileno(fp.get())));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "dup";
    return false;
  }

  if (HANDLE_EINTR(ftruncate(fd.get(), static_cast<off_t>(num_bytes_))) != 0) {
    PLOG(ERROR) << "ftruncate";
    return false;
  }

  handle_.reset(embedder::PlatformHandle(fd.release()));
  return true;
}

bool RawSharedBuffer::InitFromPlatformHandle(
    embedder::ScopedPlatformHandle platform_handle) {
  DCHECK(!handle_.is_valid());

  if (static_cast<uint64_t>(num_bytes_) >
          static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
    return false;
  }

  struct stat sb = {};
  // Note: |fstat()| isn't interruptible.
  if (fstat(platform_handle.get().fd, &sb) != 0) {
    PLOG(ERROR) << "fstat";
    return false;
  }

  if (!S_ISREG(sb.st_mode)) {
    LOG(ERROR) << "Platform handle not to a regular file";
    return false;
  }

  if (sb.st_size != static_cast<off_t>(num_bytes_)) {
    LOG(ERROR) << "Shared memory file has the wrong size";
    return false;
  }

  // TODO(vtl): More checks?

  handle_ = platform_handle.Pass();
  return true;
}

scoped_ptr<RawSharedBufferMapping> RawSharedBuffer::MapImpl(size_t offset,
                                                            size_t length) {
  size_t offset_rounding = offset % base::SysInfo::VMAllocationGranularity();
  size_t real_offset = offset - offset_rounding;
  size_t real_length = length + offset_rounding;

  // This should hold (since we checked |num_bytes| versus the maximum value of
  // |off_t| on creation, but it never hurts to be paranoid.
  DCHECK_LE(static_cast<uint64_t>(real_offset),
            static_cast<uint64_t>(std::numeric_limits<off_t>::max()));

  void* real_base = mmap(NULL, real_length, PROT_READ | PROT_WRITE, MAP_SHARED,
                         handle_.get().fd, static_cast<off_t>(real_offset));
  // |mmap()| should return |MAP_FAILED| (a.k.a. -1) on error. But it shouldn't
  // return null either.
  if (real_base == MAP_FAILED || !real_base) {
    PLOG(ERROR) << "mmap";
    return scoped_ptr<RawSharedBufferMapping>();
  }

  void* base = static_cast<char*>(real_base) + offset_rounding;
  return make_scoped_ptr(
      new RawSharedBufferMapping(base, length, real_base, real_length));
}

// RawSharedBufferMapping ------------------------------------------------------

void RawSharedBufferMapping::Unmap() {
  int result = munmap(real_base_, real_length_);
  PLOG_IF(ERROR, result != 0) << "munmap";
}

}  // namespace system
}  // namespace mojo
