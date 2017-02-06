// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"

namespace gpu {
namespace gles2 {
class MemoryTracker;
}

class GPU_EXPORT TransferBufferManagerInterface :
    public base::RefCounted<TransferBufferManagerInterface> {
 public:
  virtual bool RegisterTransferBuffer(
      int32_t id,
      std::unique_ptr<BufferBacking> buffer) = 0;
  virtual void DestroyTransferBuffer(int32_t id) = 0;
  virtual scoped_refptr<Buffer> GetTransferBuffer(int32_t id) = 0;

 protected:
  friend class base::RefCounted<TransferBufferManagerInterface>;

  virtual ~TransferBufferManagerInterface();
};

class GPU_EXPORT TransferBufferManager
    : public TransferBufferManagerInterface,
      public base::trace_event::MemoryDumpProvider {
 public:
  explicit TransferBufferManager(gles2::MemoryTracker* memory_tracker);

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  bool Initialize();
  bool RegisterTransferBuffer(
      int32_t id,
      std::unique_ptr<BufferBacking> buffer_backing) override;
  void DestroyTransferBuffer(int32_t id) override;
  scoped_refptr<Buffer> GetTransferBuffer(int32_t id) override;

 private:
  ~TransferBufferManager() override;

  typedef base::hash_map<int32_t, scoped_refptr<Buffer>> BufferMap;
  BufferMap registered_buffers_;
  size_t shared_memory_bytes_allocated_;
  gles2::MemoryTracker* memory_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TransferBufferManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
