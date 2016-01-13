// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CONTENT_RENDERER_PEPPER_HOST_ARRAY_BUFFER_VAR_H_
#define PPAPI_CONTENT_RENDERER_PEPPER_HOST_ARRAY_BUFFER_VAR_H_

#include "base/memory/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"

namespace content {

// Represents a host-side ArrayBufferVar.
class HostArrayBufferVar : public ppapi::ArrayBufferVar {
 public:
  explicit HostArrayBufferVar(uint32 size_in_bytes);
  explicit HostArrayBufferVar(const blink::WebArrayBuffer& buffer);
  explicit HostArrayBufferVar(uint32 size_in_bytes,
                              base::SharedMemoryHandle handle);

  // ArrayBufferVar implementation.
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 ByteLength() OVERRIDE;
  virtual bool CopyToNewShmem(PP_Instance instance,
                              int* host_shm_handle_id,
                              base::SharedMemoryHandle* plugin_shm_handle)
      OVERRIDE;

  blink::WebArrayBuffer& webkit_buffer() { return buffer_; }

 private:
  virtual ~HostArrayBufferVar();

  blink::WebArrayBuffer buffer_;
  // Tracks whether the data in the buffer is valid.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(HostArrayBufferVar);
};

}  // namespace content

#endif  // PPAPI_CONTENT_RENDERER_PEPPER_HOST_ARRAY_BUFFER_VAR_H_
