// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class PortProvider;
}

namespace mojo {
namespace edk {

class ProcessDelegate;

using ProcessErrorCallback = base::Callback<void(const std::string& error)>;

// Basic configuration/initialization ------------------------------------------

// |Init()| sets up the basic Mojo system environment, making the |Mojo...()|
// functions available and functional. This is never shut down (except in tests
// -- see test_embedder.h).

// Allows changing the default max message size. Must be called before Init.
MOJO_SYSTEM_IMPL_EXPORT void SetMaxMessageSize(size_t bytes);

// Called in the parent process for each child process that is launched.
MOJO_SYSTEM_IMPL_EXPORT void ChildProcessLaunched(
    base::ProcessHandle child_process,
    ScopedPlatformHandle server_pipe,
    const std::string& child_token);

// Called in the parent process for each child process that is launched.
// |process_error_callback| is called if the system becomes aware of some
// internal error related to this process, e.g., if the system is notified of a
// bad message from this process via the |MojoNotifyBadMessage()| API.
MOJO_SYSTEM_IMPL_EXPORT void ChildProcessLaunched(
    base::ProcessHandle child_process,
    ScopedPlatformHandle server_pipe,
    const std::string& child_token,
    const ProcessErrorCallback& error_callback);

// Called in the parent process when a child process fails to launch.
// Exactly one of ChildProcessLaunched() or ChildProcessLaunchFailed() must be
// called per child process launch attempt.
MOJO_SYSTEM_IMPL_EXPORT void ChildProcessLaunchFailed(
    const std::string& child_token);

// Should be called as early as possible in the child process with the handle
// that the parent received from ChildProcessLaunched.
MOJO_SYSTEM_IMPL_EXPORT void SetParentPipeHandle(ScopedPlatformHandle pipe);

// Same as above but extracts the pipe handle from the command line. See
// PlatformChannelPair for details.
MOJO_SYSTEM_IMPL_EXPORT void SetParentPipeHandleFromCommandLine();

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init();

// Basic functions -------------------------------------------------------------

// The functions in this section are available once |Init()| has been called.

// Start waiting on the handle asynchronously. On success, |callback| will be
// called exactly once, when |handle| satisfies a signal in |signals| or it
// becomes known that it will never do so. |callback| will be executed on an
// arbitrary thread, so it must not call any Mojo system or embedder functions.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
AsyncWait(MojoHandle handle,
          MojoHandleSignals signals,
          const base::Callback<void(MojoResult)>& callback);

// Creates a |MojoHandle| that wraps the given |PlatformHandle| (taking
// ownership of it). This |MojoHandle| can then, e.g., be passed through message
// pipes. Note: This takes ownership (and thus closes) |platform_handle| even on
// failure, which is different from what you'd expect from a Mojo API, but it
// makes for a more convenient embedder API.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
CreatePlatformHandleWrapper(ScopedPlatformHandle platform_handle,
                            MojoHandle* platform_handle_wrapper_handle);

// Retrieves the |PlatformHandle| that was wrapped into a |MojoHandle| (using
// |CreatePlatformHandleWrapper()| above). Note that the |MojoHandle| is closed
// on success.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                          ScopedPlatformHandle* platform_handle);

// Creates a |MojoHandle| that wraps the given |SharedMemoryHandle| (taking
// ownership of it). |num_bytes| is the size of the shared memory object, and
// |read_only| is whether the handle is a read-only handle to shared memory.
// This |MojoHandle| is a Mojo shared buffer and can be manipulated using the
// shared buffer functions and transferred over a message pipe.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
CreateSharedBufferWrapper(base::SharedMemoryHandle shared_memory_handle,
                          size_t num_bytes,
                          bool read_only,
                          MojoHandle* mojo_wrapper_handle);

// Retrieves the underlying |SharedMemoryHandle| from a shared buffer
// |MojoHandle| and closes the handle. If successful, |num_bytes| will contain
// the size of the shared memory buffer and |read_only| will contain whether the
// buffer handle is read-only. Both |num_bytes| and |read_only| may be null.
// Note: The value of |shared_memory_handle| may be
// base::SharedMemory::NULLHandle(), even if this function returns success.
// Callers should perform appropriate checks.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
PassSharedMemoryHandle(MojoHandle mojo_handle,
                       base::SharedMemoryHandle* shared_memory_handle,
                       size_t* num_bytes,
                       bool* read_only);

// Initialialization/shutdown for interprocess communication (IPC) -------------

// |InitIPCSupport()| sets up the subsystem for interprocess communication,
// making the IPC functions (in the following section) available and functional.
// (This may only be done after |Init()|.)
//
// This subsystem may be shut down using |ShutdownIPCSupport()|. None of the IPC
// functions may be called after this is called.

// Initializes a process of the given type; to be called after |Init()|.
//   - |process_delegate| must be a process delegate of the appropriate type
//     corresponding to |process_type|; its methods will be called on the same
//     thread as Shutdown.
//   - |process_delegate|, and |io_thread_task_runner| should live at least
//     until |ShutdownIPCSupport()|'s callback has been run.
MOJO_SYSTEM_IMPL_EXPORT void InitIPCSupport(
    ProcessDelegate* process_delegate,
    scoped_refptr<base::TaskRunner> io_thread_task_runner);

// Shuts down the subsystem initialized by |InitIPCSupport()|. It be called from
// any thread and will attempt to complete shutdown on the I/O thread with which
// the system was initialized. Upon completion the ProcessDelegate's
// |OnShutdownComplete()| method is invoked.
MOJO_SYSTEM_IMPL_EXPORT void ShutdownIPCSupport();

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Set the |base::PortProvider| for this process. Can be called on any thread,
// but must be set in the root process before any Mach ports can be transferred.
MOJO_SYSTEM_IMPL_EXPORT void SetMachPortProvider(
    base::PortProvider* port_provider);
#endif

// Creates a message pipe over an arbitrary platform channel. The other end of
// the channel must also be passed to this function. Either endpoint can be in
// any process.
//
// Note that the channel is only used to negotiate pipe connection, not as the
// transport for messages on the pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateMessagePipe(ScopedPlatformHandle platform_handle);

// Creates a message pipe from a token. A child embedder must also have this
// token and call CreateChildMessagePipe() with it in order for the pipe to get
// connected. |child_token| identifies the child process and should be the same
// as the token passed into ChildProcessLaunched(). If they are different, the
// returned message pipe will not be signaled of peer closure if the child
// process dies before establishing connection to the pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateParentMessagePipe(const std::string& token,
                        const std::string& child_token);

// Creates a message pipe from a token in a child process. The parent must also
// have this token and call CreateParentMessagePipe() with it in order for the
// pipe to get connected.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateChildMessagePipe(const std::string& token);

// Generates a random ASCII token string for use with CreateParentMessagePipe()
// and CreateChildMessagePipe() above. The generated token is suitably random so
// as to not have to worry about collisions with other generated tokens.
MOJO_SYSTEM_IMPL_EXPORT std::string GenerateRandomToken();

// Sets system properties that can be read by the MojoGetProperty() API. See the
// documentation for MojoPropertyType for supported property types and their
// corresponding value type.
//
// Default property values:
//   |MOJO_PROPERTY_TYPE_SYNC_CALL_ALLOWED| - true
MOJO_SYSTEM_IMPL_EXPORT MojoResult SetProperty(MojoPropertyType type,
                                               const void* value);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_H_
