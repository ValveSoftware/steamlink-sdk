// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_EMBEDDER_H_
#define MOJO_EMBEDDER_EMBEDDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// Must be called first to initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init();

// Creates a new "channel", returning a handle to the bootstrap message pipe on
// that channel. |platform_handle| should be an OS-dependent handle to one side
// of a suitable bidirectional OS "pipe" (e.g., a file descriptor to a socket on
// POSIX, a handle to a named pipe on Windows); this "pipe" should be connected
// and ready for operation (e.g., to be written to or read from).
// |io_thread_task_runner| should be a |TaskRunner| for the thread on which the
// "channel" will run (read data and demultiplex).
//
// On completion, it will run |callback| with a pointer to a |ChannelInfo|
// (which is meant to be opaque to the embedder). If
// |callback_thread_task_runner| is non-null, it the callback will be posted to
// that task runner. Otherwise, it will be run on the I/O thread directly.
//
// Returns an invalid |MOJO_HANDLE_INVALID| on error. Note that this will happen
// only if, e.g., the handle table is full (operation of the channel begins
// asynchronously and if, e.g., the other end of the "pipe" is closed, this will
// report an error to the returned handle in the usual way).
//
// Notes: The handle returned is ready for use immediately, with messages
// written to it queued. E.g., it would be perfectly valid for a message to be
// immediately written to the returned handle and the handle closed, all before
// the channel has begun operation on the IO thread. In this case, the channel
// is expected to connect as usual, send the queued message, and report that the
// handle was closed to the other side. (This message may well contain another
// handle, so there may well still be message pipes "on" this channel.)
//
// TODO(vtl): Figure out channel teardown.
struct ChannelInfo;
typedef base::Callback<void(ChannelInfo*)> DidCreateChannelCallback;
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner);

MOJO_SYSTEM_IMPL_EXPORT void DestroyChannelOnIOThread(
    ChannelInfo* channel_info);

// Creates a |MojoHandle| that wraps the given |PlatformHandle| (taking
// ownership of it). This |MojoHandle| can then, e.g., be passed through message
// pipes. Note: This takes ownership (and thus closes) |platform_handle| even on
// failure, which is different from what you'd expect from a Mojo API, but it
// makes for a more convenient embedder API.
MOJO_SYSTEM_IMPL_EXPORT MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle);
// Retrieves the |PlatformHandle| that was wrapped into a |MojoHandle| (using
// |CreatePlatformHandleWrapper()| above). Note that the |MojoHandle| must still
// be closed separately.
MOJO_SYSTEM_IMPL_EXPORT MojoResult PassWrappedPlatformHandle(
    MojoHandle platform_handle_wrapper_handle,
    ScopedPlatformHandle* platform_handle);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_EMBEDDER_H_
