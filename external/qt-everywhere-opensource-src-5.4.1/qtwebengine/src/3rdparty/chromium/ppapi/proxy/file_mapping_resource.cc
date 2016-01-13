// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_mapping_resource.h"

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_io_api.h"

namespace ppapi {
namespace proxy {

FileMappingResource::FileMappingResource(Connection connection,
                                         PP_Instance instance)
    : PluginResource(connection, instance) {
}

FileMappingResource::~FileMappingResource() {
}

thunk::PPB_FileMapping_API* FileMappingResource::AsPPB_FileMapping_API() {
  return this;
}

int32_t FileMappingResource::Map(PP_Instance /* instance */,
                                 PP_Resource file_io,
                                 int64_t length,
                                 uint32_t protection,
                                 uint32_t flags,
                                 int64_t offset,
                                 void** address,
                                 scoped_refptr<TrackedCallback> callback) {
  thunk::EnterResourceNoLock<thunk::PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  FileIOResource* file_io_resource =
      static_cast<FileIOResource*>(enter.object());
  scoped_refptr<FileIOResource::FileHolder> file_holder =
      file_io_resource->file_holder();
  if (!FileIOResource::FileHolder::IsValid(file_holder))
    return PP_ERROR_FAILED;
  if (length < 0 || offset < 0 ||
      !base::IsValueInRangeForNumericType<off_t>(offset)) {
    return PP_ERROR_BADARGUMENT;
  }
  if (!base::IsValueInRangeForNumericType<size_t>(length)) {
    return PP_ERROR_NOMEMORY;
  }

  // Ensure any bits we don't recognize are zero.
  if (protection &
      ~(PP_FILEMAPPROTECTION_READ | PP_FILEMAPPROTECTION_WRITE)) {
    return PP_ERROR_BADARGUMENT;
  }
  if (flags &
      ~(PP_FILEMAPFLAG_SHARED | PP_FILEMAPFLAG_PRIVATE |
        PP_FILEMAPFLAG_FIXED)) {
    return PP_ERROR_BADARGUMENT;
  }
  // Ensure at least one of SHARED and PRIVATE is set.
  if (!(flags & (PP_FILEMAPFLAG_SHARED | PP_FILEMAPFLAG_PRIVATE)))
    return PP_ERROR_BADARGUMENT;
  // Ensure at most one of SHARED and PRIVATE is set.
  if ((flags & PP_FILEMAPFLAG_SHARED) &&
      (flags & PP_FILEMAPFLAG_PRIVATE)) {
    return PP_ERROR_BADARGUMENT;
  }
  if (!address)
    return PP_ERROR_BADARGUMENT;

  base::Callback<MapResult()> map_cb(
      base::Bind(&FileMappingResource::DoMapBlocking, file_holder, *address,
                 length, protection, flags, offset));
  if (callback->is_blocking()) {
    // The plugin could release its reference to this instance when we release
    // the proxy lock below.
    scoped_refptr<FileMappingResource> protect(this);
    MapResult map_result;
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      map_result = map_cb.Run();
    }
    OnMapCompleted(address, length, callback, map_result);
    return map_result.result;
  } else {
    base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      map_cb,
      RunWhileLocked(Bind(&FileMappingResource::OnMapCompleted,
                          this,
                          base::Unretained(address),
                          length,
                          callback)));
    return PP_OK_COMPLETIONPENDING;
  }
}

int32_t FileMappingResource::Unmap(PP_Instance /* instance */,
                                   const void* address,
                                   int64_t length,
                                   scoped_refptr<TrackedCallback> callback) {
  if (!address)
    return PP_ERROR_BADARGUMENT;
  if (!base::IsValueInRangeForNumericType<size_t>(length))
    return PP_ERROR_BADARGUMENT;

  base::Callback<int32_t()> unmap_cb(
      base::Bind(&FileMappingResource::DoUnmapBlocking, address, length));
  if (callback->is_blocking()) {
    // Release the proxy lock while making a potentially slow file call.
    ProxyAutoUnlock unlock;
    return unmap_cb.Run();
  } else {
    base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      unmap_cb,
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
    return PP_OK_COMPLETIONPENDING;
  }
}

int64_t FileMappingResource::GetMapPageSize(PP_Instance /* instance */) {
  return DoGetMapPageSize();
}

void FileMappingResource::OnMapCompleted(
    void** mapped_address_out_param,
    int64_t length,
    scoped_refptr<TrackedCallback> callback,
    const MapResult& map_result) {
  if (callback->aborted()) {
    if (map_result.result == PP_OK) {
      // If the Map operation was successful, we need to Unmap to avoid leaks.
      // The plugin won't get the address, so doesn't have a chance to do the
      // Unmap.
      PpapiGlobals::Get()->GetFileTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&FileMappingResource::DoUnmapBlocking),
                     map_result.address,
                     length));
    }
    return;
  }
  if (map_result.result == PP_OK)
    *mapped_address_out_param = map_result.address;
  if (!callback->is_blocking())
    callback->Run(map_result.result);
}

}  // namespace proxy
}  // namespace ppapi
