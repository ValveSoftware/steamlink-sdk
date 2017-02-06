// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_handle_registry.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "mojo/public/c/system/core.h"

namespace mojo {
namespace {

base::LazyInstance<base::ThreadLocalPointer<SyncHandleRegistry>>
    g_current_sync_handle_watcher = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<SyncHandleRegistry> SyncHandleRegistry::current() {
  scoped_refptr<SyncHandleRegistry> result(
      g_current_sync_handle_watcher.Pointer()->Get());
  if (!result) {
    result = new SyncHandleRegistry();
    DCHECK_EQ(result.get(), g_current_sync_handle_watcher.Pointer()->Get());
  }
  return result;
}

bool SyncHandleRegistry::RegisterHandle(const Handle& handle,
                                        MojoHandleSignals handle_signals,
                                        const HandleCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ContainsKey(handles_, handle))
    return false;

  MojoResult result = MojoAddHandle(wait_set_handle_.get().value(),
                                    handle.value(), handle_signals);
  if (result != MOJO_RESULT_OK)
    return false;

  handles_[handle] = callback;
  return true;
}

void SyncHandleRegistry::UnregisterHandle(const Handle& handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ContainsKey(handles_, handle))
    return;

  MojoResult result =
      MojoRemoveHandle(wait_set_handle_.get().value(), handle.value());
  DCHECK_EQ(MOJO_RESULT_OK, result);
  handles_.erase(handle);
}

bool SyncHandleRegistry::WatchAllHandles(const bool* should_stop[],
                                         size_t count) {
  DCHECK(thread_checker_.CalledOnValidThread());

  MojoResult result;
  uint32_t num_ready_handles;
  MojoHandle ready_handle;
  MojoResult ready_handle_result;

  scoped_refptr<SyncHandleRegistry> preserver(this);
  while (true) {
    for (size_t i = 0; i < count; ++i)
      if (*should_stop[i])
        return true;
    do {
      result = Wait(wait_set_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, nullptr);
      if (result != MOJO_RESULT_OK)
        return false;

      // TODO(yzshen): Theoretically it can reduce sync call re-entrancy if we
      // give priority to the handle that is waiting for sync response.
      num_ready_handles = 1;
      result = MojoGetReadyHandles(wait_set_handle_.get().value(),
                                   &num_ready_handles, &ready_handle,
                                   &ready_handle_result, nullptr);
      if (result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT)
        return false;
    } while (result == MOJO_RESULT_SHOULD_WAIT);

    const auto iter = handles_.find(Handle(ready_handle));
    iter->second.Run(ready_handle_result);
  };

  return false;
}

SyncHandleRegistry::SyncHandleRegistry() {
  MojoHandle handle;
  MojoResult result = MojoCreateWaitSet(&handle);
  CHECK_EQ(MOJO_RESULT_OK, result);
  wait_set_handle_.reset(Handle(handle));
  CHECK(wait_set_handle_.is_valid());

  DCHECK(!g_current_sync_handle_watcher.Pointer()->Get());
  g_current_sync_handle_watcher.Pointer()->Set(this);
}

SyncHandleRegistry::~SyncHandleRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_current_sync_handle_watcher.Pointer()->Set(nullptr);
}

}  // namespace mojo
