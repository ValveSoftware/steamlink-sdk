// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
#define PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

// Represents a plugin-side ArrayBufferVar. In the plugin process, it's
// owned as a vector.
class PluginArrayBufferVar : public ArrayBufferVar {
 public:
  explicit PluginArrayBufferVar(uint32 size_in_bytes);
  PluginArrayBufferVar(uint32 size_in_bytes,
                       base::SharedMemoryHandle plugin_handle);
  virtual ~PluginArrayBufferVar();

  // ArrayBufferVar implementation.
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 ByteLength() OVERRIDE;
  virtual bool CopyToNewShmem(
      PP_Instance instance,
      int* host_handle,
      base::SharedMemoryHandle* plugin_handle) OVERRIDE;

 private:
  // Non-shared memory
  std::vector<uint8> buffer_;

  // Shared memory
  base::SharedMemoryHandle plugin_handle_;
  scoped_ptr<base::SharedMemory> shmem_;
  uint32 size_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(PluginArrayBufferVar);
};

}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
