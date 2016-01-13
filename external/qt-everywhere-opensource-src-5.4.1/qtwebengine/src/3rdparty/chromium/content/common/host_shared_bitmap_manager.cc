// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_shared_bitmap_manager.h"

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "content/common/view_messages.h"
#include "ui/gfx/size.h"

namespace content {

class BitmapData : public base::RefCountedThreadSafe<BitmapData> {
 public:
  BitmapData(base::ProcessHandle process_handle,
             base::SharedMemoryHandle memory_handle,
             size_t buffer_size)
      : process_handle(process_handle),
        memory_handle(memory_handle),
        buffer_size(buffer_size) {}
  base::ProcessHandle process_handle;
  base::SharedMemoryHandle memory_handle;
  scoped_ptr<base::SharedMemory> memory;
  scoped_ptr<uint8[]> pixels;
  size_t buffer_size;

 private:
  friend class base::RefCountedThreadSafe<BitmapData>;
  ~BitmapData() {}
  DISALLOW_COPY_AND_ASSIGN(BitmapData);
};

// Holds a reference on the memory to keep it alive.
void FreeSharedMemory(scoped_refptr<BitmapData> data,
                      cc::SharedBitmap* bitmap) {}

base::LazyInstance<HostSharedBitmapManager> g_shared_memory_manager =
    LAZY_INSTANCE_INITIALIZER;

HostSharedBitmapManager::HostSharedBitmapManager() {}
HostSharedBitmapManager::~HostSharedBitmapManager() {}

HostSharedBitmapManager* HostSharedBitmapManager::current() {
  return g_shared_memory_manager.Pointer();
}

scoped_ptr<cc::SharedBitmap> HostSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size))
    return scoped_ptr<cc::SharedBitmap>();

  scoped_refptr<BitmapData> data(
      new BitmapData(base::GetCurrentProcessHandle(),
                     base::SharedMemory::NULLHandle(),
                     bitmap_size));
  // Bitmaps allocated in host don't need to be shared to other processes, so
  // allocate them with new instead.
  data->pixels = scoped_ptr<uint8[]>(new uint8[bitmap_size]);

  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  handle_map_[id] = data;
  return make_scoped_ptr(new cc::SharedBitmap(
      data->pixels.get(),
      id,
      base::Bind(&HostSharedBitmapManager::FreeSharedMemoryFromMap,
                 base::Unretained(this))));
}

scoped_ptr<cc::SharedBitmap> HostSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size& size,
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  BitmapMap::iterator it = handle_map_.find(id);
  if (it == handle_map_.end())
    return scoped_ptr<cc::SharedBitmap>();

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size) ||
      bitmap_size > data->buffer_size)
    return scoped_ptr<cc::SharedBitmap>();

  if (data->pixels) {
    return make_scoped_ptr(new cc::SharedBitmap(
        data->pixels.get(), id, base::Bind(&FreeSharedMemory, it->second)));
  }
  if (!data->memory->memory()) {
    TRACE_EVENT0("renderer_host",
                 "HostSharedBitmapManager::GetSharedBitmapFromId");
    if (!data->memory->Map(data->buffer_size)) {
      return scoped_ptr<cc::SharedBitmap>();
    }
  }

  scoped_ptr<cc::SharedBitmap> bitmap(new cc::SharedBitmap(
      data->memory.get(), id, base::Bind(&FreeSharedMemory, it->second)));

  return bitmap.Pass();
}

scoped_ptr<cc::SharedBitmap> HostSharedBitmapManager::GetBitmapForSharedMemory(
    base::SharedMemory*) {
  return scoped_ptr<cc::SharedBitmap>();
}

void HostSharedBitmapManager::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    base::ProcessHandle process_handle,
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end())
    return;
  scoped_refptr<BitmapData> data(
      new BitmapData(process_handle, handle, buffer_size));

  handle_map_[id] = data;
  process_map_[process_handle].insert(id);
#if defined(OS_WIN)
  data->memory = make_scoped_ptr(
      new base::SharedMemory(data->memory_handle, false, data->process_handle));
#else
  data->memory =
      make_scoped_ptr(new base::SharedMemory(data->memory_handle, false));
#endif
}

void HostSharedBitmapManager::AllocateSharedBitmapForChild(
    base::ProcessHandle process_handle,
    size_t buffer_size,
    const cc::SharedBitmapId& id,
    base::SharedMemoryHandle* shared_memory_handle) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end()) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAndMapAnonymous(buffer_size)) {
    LOG(ERROR) << "Cannot create shared memory buffer";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  scoped_refptr<BitmapData> data(
      new BitmapData(process_handle, shared_memory->handle(), buffer_size));
  data->memory = shared_memory.Pass();

  handle_map_[id] = data;
  process_map_[process_handle].insert(id);
  if (!data->memory->ShareToProcess(process_handle, shared_memory_handle)) {
    LOG(ERROR) << "Cannot share shared memory buffer";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }
}

void HostSharedBitmapManager::ChildDeletedSharedBitmap(
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  BitmapMap::iterator it = handle_map_.find(id);
  if (it == handle_map_.end())
    return;
  base::hash_set<cc::SharedBitmapId>& res =
      process_map_[it->second->process_handle];
  res.erase(id);
  handle_map_.erase(it);
}

void HostSharedBitmapManager::ProcessRemoved(
    base::ProcessHandle process_handle) {
  base::AutoLock lock(lock_);
  ProcessMap::iterator proc_it = process_map_.find(process_handle);
  if (proc_it == process_map_.end())
    return;
  base::hash_set<cc::SharedBitmapId>& res = proc_it->second;

  for (base::hash_set<cc::SharedBitmapId>::iterator it = res.begin();
       it != res.end();
       ++it) {
    handle_map_.erase(*it);
  }
  process_map_.erase(proc_it);
}

void HostSharedBitmapManager::FreeSharedMemoryFromMap(
    cc::SharedBitmap* bitmap) {
  base::AutoLock lock(lock_);
  handle_map_.erase(bitmap->id());
}

}  // namespace content
