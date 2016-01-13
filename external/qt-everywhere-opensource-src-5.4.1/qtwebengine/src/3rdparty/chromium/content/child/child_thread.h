// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_THREAD_H_
#define CONTENT_CHILD_CHILD_THREAD_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/tracked_objects.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace base {
class MessageLoop;

namespace debug {
class TraceMemoryController;
}  // namespace debug
}  // namespace base

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}  // namespace IPC

namespace blink {
class WebFrame;
}  // namespace blink

namespace webkit_glue {
class ResourceLoaderBridge;
}  // namespace webkit_glue

namespace content {
class ChildHistogramMessageFilter;
class ChildResourceMessageFilter;
class ChildSharedBitmapManager;
class FileSystemDispatcher;
class MojoApplication;
class ServiceWorkerDispatcher;
class ServiceWorkerMessageFilter;
class QuotaDispatcher;
class QuotaMessageFilter;
class ResourceDispatcher;
class SocketStreamDispatcher;
class ThreadSafeSender;
class WebSocketDispatcher;
struct RequestInfo;

// The main thread of a child process derives from this class.
class CONTENT_EXPORT ChildThread
    : public IPC::Listener,
      public IPC::Sender,
      public NON_EXPORTED_BASE(mojo::ServiceProvider) {
 public:
  // Creates the thread.
  ChildThread();
  // Used for single-process mode and for in process gpu mode.
  explicit ChildThread(const std::string& channel_name);
  // ChildProcess::main_thread() is reset after Shutdown(), and before the
  // destructor, so any subsystem that relies on ChildProcess::main_thread()
  // must be terminated before Shutdown returns. In particular, if a subsystem
  // has a thread that post tasks to ChildProcess::main_thread(), that thread
  // should be joined in Shutdown().
  virtual ~ChildThread();
  virtual void Shutdown();

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  IPC::SyncChannel* channel() { return channel_.get(); }

  MessageRouter* GetRouter();

  // Allocates a block of shared memory of the given size and
  // maps in into the address space. Returns NULL of failure.
  // Note: On posix, this requires a sync IPC to the browser process,
  // but on windows the child process directly allocates the block.
  base::SharedMemory* AllocateSharedMemory(size_t buf_size);

  // A static variant that can be called on background threads provided
  // the |sender| passed in is safe to use on background threads.
  static base::SharedMemory* AllocateSharedMemory(size_t buf_size,
                                                  IPC::Sender* sender);

  ChildSharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_.get();
  }

  ResourceDispatcher* resource_dispatcher() const {
    return resource_dispatcher_.get();
  }

  SocketStreamDispatcher* socket_stream_dispatcher() const {
    return socket_stream_dispatcher_.get();
  }

  WebSocketDispatcher* websocket_dispatcher() const {
    return websocket_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  ServiceWorkerDispatcher* service_worker_dispatcher() const {
    return service_worker_dispatcher_.get();
  }

  QuotaDispatcher* quota_dispatcher() const {
    return quota_dispatcher_.get();
  }

  IPC::SyncMessageFilter* sync_message_filter() const {
    return sync_message_filter_.get();
  }

  // The getter should only be called on the main thread, however the
  // IPC::Sender it returns may be safely called on any thread including
  // the main thread.
  ThreadSafeSender* thread_safe_sender() const {
    return thread_safe_sender_.get();
  }

  ChildHistogramMessageFilter* child_histogram_message_filter() const {
    return histogram_message_filter_.get();
  }

  ServiceWorkerMessageFilter* service_worker_message_filter() const {
    return service_worker_message_filter_.get();
  }

  QuotaMessageFilter* quota_message_filter() const {
    return quota_message_filter_.get();
  }

  base::MessageLoop* message_loop() const { return message_loop_; }

  // Returns the one child thread. Can only be called on the main thread.
  static ChildThread* current();

#if defined(OS_ANDROID)
  // Called on Android's service thread to shutdown the main thread of this
  // process.
  static void ShutdownThread();
#endif

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  void set_on_channel_error_called(bool on_channel_error_called) {
    on_channel_error_called_ = on_channel_error_called;
  }

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // mojo::ServiceProvider implementation:
  virtual void ConnectToService(
      const mojo::String& service_url,
      const mojo::String& service_name,
      mojo::ScopedMessagePipeHandle message_pipe,
      const mojo::String& requestor_url) OVERRIDE;

 private:
  class ChildThreadMessageRouter : public MessageRouter {
   public:
    // |sender| must outlive this object.
    explicit ChildThreadMessageRouter(IPC::Sender* sender);
    virtual bool Send(IPC::Message* msg) OVERRIDE;

   private:
    IPC::Sender* const sender_;
  };

  void Init();

  // IPC message handlers.
  void OnShutdown();
  void OnSetProfilerStatus(tracked_objects::ThreadData::Status status);
  void OnGetChildProfilerData(int sequence_number);
  void OnDumpHandles();
  void OnProcessBackgrounded(bool background);
#ifdef IPC_MESSAGE_LOG_ENABLED
  void OnSetIPCLoggingEnabled(bool enable);
#endif
#if defined(USE_TCMALLOC)
  void OnGetTcmallocStats();
#endif

  void EnsureConnected();

  scoped_ptr<MojoApplication> mojo_application_;

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Implements message routing functionality to the consumers of ChildThread.
  ChildThreadMessageRouter router_;

  // Handles resource loads for this process.
  scoped_ptr<ResourceDispatcher> resource_dispatcher_;

  // Handles SocketStream for this process.
  scoped_ptr<SocketStreamDispatcher> socket_stream_dispatcher_;

  scoped_ptr<WebSocketDispatcher> websocket_dispatcher_;

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  base::MessageLoop* message_loop_;

  scoped_ptr<FileSystemDispatcher> file_system_dispatcher_;

  scoped_ptr<ServiceWorkerDispatcher> service_worker_dispatcher_;

  scoped_ptr<QuotaDispatcher> quota_dispatcher_;

  scoped_refptr<ChildHistogramMessageFilter> histogram_message_filter_;

  scoped_refptr<ChildResourceMessageFilter> resource_message_filter_;

  scoped_refptr<ServiceWorkerMessageFilter> service_worker_message_filter_;

  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_ptr<ChildSharedBitmapManager> shared_bitmap_manager_;

  base::WeakPtrFactory<ChildThread> channel_connected_factory_;

  // Observes the trace event system. When tracing is enabled, optionally
  // starts profiling the tcmalloc heap.
  scoped_ptr<base::debug::TraceMemoryController> trace_memory_controller_;

  scoped_ptr<base::PowerMonitor> power_monitor_;

  bool in_browser_process_;

  DISALLOW_COPY_AND_ASSIGN(ChildThread);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_THREAD_H_
