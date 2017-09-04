// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_
#define CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cc::SharedBitmapId> {
  size_t operator()(const cc::SharedBitmapId& id) const {
    return base::Hash(reinterpret_cast<const char*>(id.name), sizeof(id.name));
  }
};
}  // namespace BASE_HASH_NAMESPACE

namespace content {
class BitmapData;
class HostSharedBitmapManager;

class CONTENT_EXPORT HostSharedBitmapManagerClient {
 public:
  explicit HostSharedBitmapManagerClient(HostSharedBitmapManager* manager);

  ~HostSharedBitmapManagerClient();

  void AllocateSharedBitmapForChild(
      base::ProcessHandle process_handle,
      size_t buffer_size,
      const cc::SharedBitmapId& id,
      base::SharedMemoryHandle* shared_memory_handle);
  void ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const cc::SharedBitmapId& id);
  void ChildDeletedSharedBitmap(const cc::SharedBitmapId& id);

 private:
  HostSharedBitmapManager* manager_;

  // Lock must be held around access to owned_bitmaps_.
  base::Lock lock_;
  base::hash_set<cc::SharedBitmapId> owned_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(HostSharedBitmapManagerClient);
};

class CONTENT_EXPORT HostSharedBitmapManager
    : public cc::SharedBitmapManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  HostSharedBitmapManager();
  ~HostSharedBitmapManager() override;

  static HostSharedBitmapManager* current();

  // cc::SharedBitmapManager implementation.
  std::unique_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  std::unique_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      const cc::SharedBitmapId&) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t AllocatedBitmapCount() const;

  void FreeSharedMemoryFromMap(const cc::SharedBitmapId& id);

 private:
  friend class HostSharedBitmapManagerClient;

  void AllocateSharedBitmapForChild(
      base::ProcessHandle process_handle,
      size_t buffer_size,
      const cc::SharedBitmapId& id,
      base::SharedMemoryHandle* shared_memory_handle);
  bool ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const cc::SharedBitmapId& id);
  void ChildDeletedSharedBitmap(const cc::SharedBitmapId& id);

  mutable base::Lock lock_;

  typedef base::hash_map<cc::SharedBitmapId, scoped_refptr<BitmapData> >
      BitmapMap;
  BitmapMap handle_map_;

  DISALLOW_COPY_AND_ASSIGN(HostSharedBitmapManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_
