// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/entrypoints.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/system/core.h"

#if !defined(OS_NACL)
#include "crypto/random.h"
#endif

namespace mojo {
namespace edk {

class Core;
class PlatformSupport;

namespace internal {

Core* g_core;
ProcessDelegate* g_process_delegate;

Core* GetCore() { return g_core; }

}  // namespace internal

void SetMaxMessageSize(size_t bytes) {
}

void ChildProcessLaunched(base::ProcessHandle child_process,
                          ScopedPlatformHandle server_pipe,
                          const std::string& child_token) {
  ChildProcessLaunched(child_process, std::move(server_pipe),
                       child_token, ProcessErrorCallback());
}

void ChildProcessLaunched(base::ProcessHandle child_process,
                          ScopedPlatformHandle server_pipe,
                          const std::string& child_token,
                          const ProcessErrorCallback& process_error_callback) {
  CHECK(internal::g_core);
  internal::g_core->AddChild(child_process, std::move(server_pipe),
                             child_token, process_error_callback);
}

void ChildProcessLaunchFailed(const std::string& child_token) {
  CHECK(internal::g_core);
  internal::g_core->ChildLaunchFailed(child_token);
}

void SetParentPipeHandle(ScopedPlatformHandle pipe) {
  CHECK(internal::g_core);
  internal::g_core->InitChild(std::move(pipe));
}

void SetParentPipeHandleFromCommandLine() {
  ScopedPlatformHandle platform_channel =
      PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess());
  CHECK(platform_channel.is_valid());
  SetParentPipeHandle(std::move(platform_channel));
}

void Init() {
  MojoSystemThunks thunks = MakeSystemThunks();
  size_t expected_size = MojoEmbedderSetSystemThunks(&thunks);
  DCHECK_EQ(expected_size, sizeof(thunks));

  internal::g_core = new Core();
}

MojoResult AsyncWait(MojoHandle handle,
                     MojoHandleSignals signals,
                     const base::Callback<void(MojoResult)>& callback) {
  CHECK(internal::g_core);
  return internal::g_core->AsyncWait(handle, signals, callback);
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  return internal::g_core->CreatePlatformHandleWrapper(
      std::move(platform_handle), platform_handle_wrapper_handle);
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  return internal::g_core->PassWrappedPlatformHandle(
      platform_handle_wrapper_handle, platform_handle);
}

MojoResult CreateSharedBufferWrapper(
    base::SharedMemoryHandle shared_memory_handle,
    size_t num_bytes,
    bool read_only,
    MojoHandle* mojo_wrapper_handle) {
  return internal::g_core->CreateSharedBufferWrapper(
      shared_memory_handle, num_bytes, read_only, mojo_wrapper_handle);
}

MojoResult PassSharedMemoryHandle(
    MojoHandle mojo_handle,
    base::SharedMemoryHandle* shared_memory_handle,
    size_t* num_bytes,
    bool* read_only) {
  return internal::g_core->PassSharedMemoryHandle(
      mojo_handle, shared_memory_handle, num_bytes, read_only);
}

void InitIPCSupport(ProcessDelegate* process_delegate,
                    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  CHECK(internal::g_core);
  internal::g_core->SetIOTaskRunner(io_thread_task_runner);
  internal::g_process_delegate = process_delegate;
}

void ShutdownIPCSupport() {
  CHECK(internal::g_process_delegate);
  CHECK(internal::g_core);
  internal::g_core->RequestShutdown(
      base::Bind(&ProcessDelegate::OnShutdownComplete,
                 base::Unretained(internal::g_process_delegate)));
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void SetMachPortProvider(base::PortProvider* port_provider) {
  DCHECK(port_provider);
  internal::g_core->SetMachPortProvider(port_provider);
}
#endif

ScopedMessagePipeHandle CreateMessagePipe(
    ScopedPlatformHandle platform_handle) {
  CHECK(internal::g_process_delegate);
  return internal::g_core->CreateMessagePipe(std::move(platform_handle));
}

ScopedMessagePipeHandle CreateParentMessagePipe(
    const std::string& token, const std::string& child_token) {
  CHECK(internal::g_process_delegate);
  return internal::g_core->CreateParentMessagePipe(token, child_token);
}

ScopedMessagePipeHandle CreateChildMessagePipe(const std::string& token) {
  CHECK(internal::g_process_delegate);
  return internal::g_core->CreateChildMessagePipe(token);
}

std::string GenerateRandomToken() {
  char random_bytes[16];
#if defined(OS_NACL)
  // Not secure. For NaCl only!
  base::RandBytes(random_bytes, 16);
#else
  crypto::RandBytes(random_bytes, 16);
#endif
  return base::HexEncode(random_bytes, 16);
}

MojoResult SetProperty(MojoPropertyType type, const void* value) {
  CHECK(internal::g_core);
  return internal::g_core->SetProperty(type, value);
}

}  // namespace edk
}  // namespace mojo
