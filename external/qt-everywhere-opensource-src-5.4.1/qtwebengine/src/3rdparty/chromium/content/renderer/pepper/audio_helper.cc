// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/audio_helper.h"

#include "base/logging.h"
#include "content/renderer/pepper/common.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"

using ppapi::TrackedCallback;

namespace content {

// AudioHelper -----------------------------------------------------------------

AudioHelper::AudioHelper() : shared_memory_size_for_create_callback_(0) {}

AudioHelper::~AudioHelper() {}

int32_t AudioHelper::GetSyncSocketImpl(int* sync_socket) {
  if (socket_for_create_callback_) {
#if defined(OS_POSIX)
    *sync_socket = socket_for_create_callback_->handle();
#elif defined(OS_WIN)
    *sync_socket = reinterpret_cast<int>(socket_for_create_callback_->handle());
#else
#error "Platform not supported."
#endif
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

int32_t AudioHelper::GetSharedMemoryImpl(int* shm_handle, uint32_t* shm_size) {
  if (shared_memory_for_create_callback_) {
#if defined(OS_POSIX)
    *shm_handle = shared_memory_for_create_callback_->handle().fd;
#elif defined(OS_WIN)
    *shm_handle =
        reinterpret_cast<int>(shared_memory_for_create_callback_->handle());
#else
#error "Platform not supported."
#endif
    *shm_size = shared_memory_size_for_create_callback_;
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

void AudioHelper::StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                                size_t shared_memory_size,
                                base::SyncSocket::Handle socket_handle) {
  if (TrackedCallback::IsPending(create_callback_)) {
    // Trusted side of proxy can specify a callback to receive handles. In
    // this case we don't need to map any data or start the thread since it
    // will be handled by the proxy.
    shared_memory_for_create_callback_.reset(
        new base::SharedMemory(shared_memory_handle, false));
    shared_memory_size_for_create_callback_ = shared_memory_size;
    socket_for_create_callback_.reset(new base::SyncSocket(socket_handle));

    create_callback_->Run(PP_OK);

    // It might be nice to close the handles here to free up some system
    // resources, but we can't since there's a race condition. The handles must
    // be valid until they're sent over IPC, which is done from the I/O thread
    // which will often get done after this code executes. We could do
    // something more elaborate like an ACK from the plugin or post a task to
    // the I/O thread and back, but this extra complexity doesn't seem worth it
    // just to clean up these handles faster.
  } else {
    OnSetStreamInfo(shared_memory_handle, shared_memory_size, socket_handle);
  }
}

void AudioHelper::SetCreateCallback(
    scoped_refptr<ppapi::TrackedCallback> create_callback) {
  DCHECK(!TrackedCallback::IsPending(create_callback_));
  create_callback_ = create_callback;
}

}  // namespace content
