// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_
#define CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)
template <>
struct hash<cc::SharedBitmapId> {
  size_t operator()(const cc::SharedBitmapId& id) const {
    return base::Hash(reinterpret_cast<const char*>(id.name), sizeof(id.name));
  }
};
#elif defined(COMPILER_MSVC)
inline std::size_t hash_value(const cc::SharedBitmapId& id) {
  return base::Hash(reinterpret_cast<const char*>(id.name), sizeof(id.name));
}
#endif  // COMPILER
}  // namespace BASE_HASH_NAMESPACE

namespace content {
class BitmapData;

class CONTENT_EXPORT HostSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  HostSharedBitmapManager();
  virtual ~HostSharedBitmapManager();

  static HostSharedBitmapManager* current();

  // cc::SharedBitmapManager implementation.
  virtual scoped_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) OVERRIDE;
  virtual scoped_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      const cc::SharedBitmapId&) OVERRIDE;
  virtual scoped_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory*) OVERRIDE;

  void AllocateSharedBitmapForChild(
      base::ProcessHandle process_handle,
      size_t buffer_size,
      const cc::SharedBitmapId& id,
      base::SharedMemoryHandle* shared_memory_handle);
  void ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  base::ProcessHandle process_handle,
                                  const cc::SharedBitmapId& id);
  void ChildDeletedSharedBitmap(const cc::SharedBitmapId& id);
  void ProcessRemoved(base::ProcessHandle process_handle);

  size_t AllocatedBitmapCount() const { return handle_map_.size(); }

 private:
  void FreeSharedMemoryFromMap(cc::SharedBitmap* bitmap);

  base::Lock lock_;

  typedef base::hash_map<cc::SharedBitmapId, scoped_refptr<BitmapData> >
      BitmapMap;
  BitmapMap handle_map_;

  typedef base::hash_map<base::ProcessHandle,
                         base::hash_set<cc::SharedBitmapId> > ProcessMap;
  ProcessMap process_map_;
};

}  // namespace content

#endif  // CONTENT_COMMON_HOST_SHARED_BITMAP_MANAGER_H_
