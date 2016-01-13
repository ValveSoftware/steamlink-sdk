// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_shared_bitmap_manager.h"

#include "content/child/child_thread.h"
#include "content/common/child_process_messages.h"
#include "ui/gfx/size.h"

namespace content {

namespace {

void FreeSharedMemory(scoped_refptr<ThreadSafeSender> sender,
                      cc::SharedBitmap* bitmap) {
  TRACE_EVENT0("renderer", "ChildSharedBitmapManager::FreeSharedMemory");
  sender->Send(new ChildProcessHostMsg_DeletedSharedBitmap(bitmap->id()));
  delete bitmap->memory();
}

void ReleaseSharedBitmap(scoped_refptr<ThreadSafeSender> sender,
                         cc::SharedBitmap* handle) {
  TRACE_EVENT0("renderer", "ChildSharedBitmapManager::ReleaseSharedBitmap");
  sender->Send(new ChildProcessHostMsg_DeletedSharedBitmap(handle->id()));
}

}  // namespace

ChildSharedBitmapManager::ChildSharedBitmapManager(
    scoped_refptr<ThreadSafeSender> sender)
    : sender_(sender) {
}

ChildSharedBitmapManager::~ChildSharedBitmapManager() {}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  TRACE_EVENT2("renderer",
               "ChildSharedBitmapManager::AllocateSharedMemory",
               "width",
               size.width(),
               "height",
               size.height());
  size_t memory_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &memory_size))
    return scoped_ptr<cc::SharedBitmap>();
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  scoped_ptr<base::SharedMemory> memory;
#if defined(OS_POSIX)
  base::SharedMemoryHandle handle;
  sender_->Send(new ChildProcessHostMsg_SyncAllocateSharedBitmap(
      memory_size, id, &handle));
  memory = make_scoped_ptr(new base::SharedMemory(handle, false));
  CHECK(memory->Map(memory_size));
#else
  memory.reset(ChildThread::AllocateSharedMemory(memory_size, sender_));
  CHECK(memory);
  base::SharedMemoryHandle handle_to_send = memory->handle();
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      memory_size, handle_to_send, id));
#endif
  return scoped_ptr<cc::SharedBitmap>(new cc::SharedBitmap(
      memory.release(), id, base::Bind(&FreeSharedMemory, sender_)));
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const cc::SharedBitmapId&) {
  NOTREACHED();
  return scoped_ptr<cc::SharedBitmap>();
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetBitmapForSharedMemory(
    base::SharedMemory* mem) {
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  base::SharedMemoryHandle handle_to_send = mem->handle();
#if defined(OS_POSIX)
  if (!mem->ShareToProcess(base::GetCurrentProcessHandle(), &handle_to_send))
    return scoped_ptr<cc::SharedBitmap>();
#endif
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      mem->mapped_size(), handle_to_send, id));
  // The compositor owning the SharedBitmap will be closed before the
  // ChildThread containng this, making the use of base::Unretained safe.
  return scoped_ptr<cc::SharedBitmap>(
      new cc::SharedBitmap(mem, id, base::Bind(&ReleaseSharedBitmap, sender_)));
}

}  // namespace content
