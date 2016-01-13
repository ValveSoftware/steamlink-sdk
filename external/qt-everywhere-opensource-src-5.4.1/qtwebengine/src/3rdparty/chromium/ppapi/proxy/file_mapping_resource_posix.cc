// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_mapping_resource.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace proxy {

namespace {

int32_t ErrnoToPPError(int error_code) {
  switch (error_code) {
    case EACCES:
      return PP_ERROR_NOACCESS;
    case EAGAIN:
      return PP_ERROR_NOMEMORY;
    case EINVAL:
      return PP_ERROR_BADARGUMENT;
    case ENFILE:
    case ENOMEM:
      return PP_ERROR_NOMEMORY;
    default:
      return PP_ERROR_FAILED;
  }
}

}  // namespace

// static
FileMappingResource::MapResult FileMappingResource::DoMapBlocking(
    scoped_refptr<FileIOResource::FileHolder> file_holder,
    void* address_hint,
    int64_t length,
    uint32_t map_protection,
    uint32_t map_flags,
    int64_t offset) {
  int prot_for_mmap = 0;
  if (map_protection & PP_FILEMAPPROTECTION_READ)
    prot_for_mmap |= PROT_READ;
  if (map_protection & PP_FILEMAPPROTECTION_WRITE)
    prot_for_mmap |= PROT_WRITE;
  if (prot_for_mmap == 0)
    prot_for_mmap = PROT_NONE;

  int flags_for_mmap = 0;
  if (map_flags & PP_FILEMAPFLAG_SHARED)
    flags_for_mmap |= MAP_SHARED;
  if (map_flags & PP_FILEMAPFLAG_PRIVATE)
    flags_for_mmap |= MAP_PRIVATE;
  if (map_flags & PP_FILEMAPFLAG_FIXED)
    flags_for_mmap |= MAP_FIXED;

  MapResult map_result;
  map_result.address =
      mmap(address_hint,
           static_cast<size_t>(length),
           prot_for_mmap,
           flags_for_mmap,
           file_holder->file()->GetPlatformFile(),
           static_cast<off_t>(offset));
  if (map_result.address != MAP_FAILED)
    map_result.result = PP_OK;
  else
    map_result.result = ErrnoToPPError(errno);
  return map_result;
}

// static
int32_t FileMappingResource::DoUnmapBlocking(const void* address,
                                             int64_t length) {
  if (munmap(const_cast<void*>(address), static_cast<size_t>(length)))
    return ErrnoToPPError(errno);
  return PP_OK;
}

// static
int64_t FileMappingResource::DoGetMapPageSize() {
  return getpagesize();
}

}  // namespace proxy
}  // namespace ppapi
