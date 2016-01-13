// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_array_buffer_var.h"

#include <stdio.h>
#include <string.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "content/common/sandbox_util.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_instance.h"

using ppapi::ArrayBufferVar;
using blink::WebArrayBuffer;

namespace content {

HostArrayBufferVar::HostArrayBufferVar(uint32 size_in_bytes)
    : buffer_(WebArrayBuffer::create(size_in_bytes, 1 /* element_size */)),
      valid_(true) {}

HostArrayBufferVar::HostArrayBufferVar(const WebArrayBuffer& buffer)
    : buffer_(buffer), valid_(true) {}

HostArrayBufferVar::HostArrayBufferVar(uint32 size_in_bytes,
                                       base::SharedMemoryHandle handle)
    : buffer_(WebArrayBuffer::create(size_in_bytes, 1 /* element_size */)) {
  base::SharedMemory s(handle, true);
  valid_ = s.Map(size_in_bytes);
  if (valid_) {
    memcpy(buffer_.data(), s.memory(), size_in_bytes);
    s.Unmap();
  }
}

HostArrayBufferVar::~HostArrayBufferVar() {}

void* HostArrayBufferVar::Map() {
  if (!valid_)
    return NULL;
  return buffer_.data();
}

void HostArrayBufferVar::Unmap() {
  // We do not used shared memory on the host side. Nothing to do.
}

uint32 HostArrayBufferVar::ByteLength() { return buffer_.byteLength(); }

bool HostArrayBufferVar::CopyToNewShmem(
    PP_Instance instance,
    int* host_shm_handle_id,
    base::SharedMemoryHandle* plugin_shm_handle) {
  scoped_ptr<base::SharedMemory> shm(
      RenderThread::Get()
          ->HostAllocateSharedMemoryBuffer(ByteLength())
          .release());
  if (!shm)
    return false;

  shm->Map(ByteLength());
  memcpy(shm->memory(), Map(), ByteLength());
  shm->Unmap();

  // Duplicate the handle here; the SharedMemory destructor closes
  // its handle on us.
  HostGlobals* hg = HostGlobals::Get();
  PluginModule* pm = hg->GetModule(hg->GetModuleForInstance(instance));
  base::ProcessId p = pm->GetPeerProcessId();
  if (p == base::kNullProcessId) {
    // In-process, clone for ourselves.
    p = base::GetCurrentProcId();
  }

  base::PlatformFile platform_file =
#if defined(OS_WIN)
      shm->handle();
#elif defined(OS_POSIX)
      shm->handle().fd;
#else
#error Not implemented.
#endif

  *plugin_shm_handle = BrokerGetFileHandleForProcess(platform_file, p, false);
  *host_shm_handle_id = -1;
  return true;
}

}  // namespace content
