// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_listener.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class FilePath;
}

namespace IPC {
class MessageFilter;
}

namespace gpu {
struct SyncToken;
}

namespace content {
class ChildProcessHostDelegate;

// Provides common functionality for hosting a child process and processing IPC
// messages between the host and the child process. Users are responsible
// for the actual launching and terminating of the child processes.
class CONTENT_EXPORT ChildProcessHostImpl : public ChildProcessHost,
                                            public IPC::Listener {
 public:
  ~ChildProcessHostImpl() override;

  // Public and static for reuse by RenderMessageFilter.
  static void AllocateSharedMemory(
      size_t buffer_size, base::ProcessHandle child_process,
      base::SharedMemoryHandle* handle);

  // Returns a unique ID to identify a child process. On construction, this
  // function will be used to generate the id_, but it is also used to generate
  // IDs for the RenderProcessHost, which doesn't inherit from us, and whose IDs
  // must be unique for all child processes.
  //
  // This function is threadsafe since RenderProcessHost is on the UI thread,
  // but normally this will be used on the IO thread.
  //
  // This will never return ChildProcessHost::kInvalidUniqueID.
  static int GenerateChildProcessUniqueId();

  // Derives a tracing process id from a child process id. Child process ids
  // cannot be used directly in child process for tracing due to security
  // reasons (see: discussion in crrev.com/1173263004). This method is meant to
  // be used when tracing for identifying cross-process shared memory from a
  // process which knows the child process id of its endpoints. The value
  // returned by this method is guaranteed to be equal to the value returned by
  // MemoryDumpManager::GetTracingProcessId() in the corresponding child
  // process.
  //
  // Never returns MemoryDumpManager::kInvalidTracingProcessId.
  // Returns only ChildProcessHost::kBrowserTracingProcessId in single-process
  // mode.
  static uint64_t ChildProcessUniqueIdToTracingProcessId(int child_process_id);

  // ChildProcessHost implementation
  bool Send(IPC::Message* message) override;
  void ForceShutdown() override;
  std::string CreateChannel() override;
  std::string CreateChannelMojo(const std::string& child_token) override;
  bool IsChannelOpening() override;
  void AddFilter(IPC::MessageFilter* filter) override;
#if defined(OS_POSIX)
  base::ScopedFD TakeClientFileDescriptor() override;
#endif

 private:
  friend class ChildProcessHost;

  explicit ChildProcessHostImpl(ChildProcessHostDelegate* delegate);

  // IPC::Listener methods:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // Message handlers:
  void OnShutdownRequest();
  void OnAllocateSharedMemory(uint32_t buffer_size,
                              base::SharedMemoryHandle* handle);
  void OnAllocateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                 uint32_t width,
                                 uint32_t height,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage,
                                 gfx::GpuMemoryBufferHandle* handle);
  void OnDeletedGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                const gpu::SyncToken& sync_token);

  // Initializes the IPC channel and returns true on success. |channel_| must be
  // non-null.
  bool InitChannel();

  ChildProcessHostDelegate* delegate_;
  base::Process peer_process_;
  bool opening_channel_;  // True while we're waiting the channel to be opened.
  std::unique_ptr<IPC::Channel> channel_;
  std::string channel_id_;

  // Holds all the IPC message filters.  Since this object lives on the IO
  // thread, we don't have a IPC::ChannelProxy and so we manage filters
  // manually.
  std::vector<scoped_refptr<IPC::MessageFilter> > filters_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_
