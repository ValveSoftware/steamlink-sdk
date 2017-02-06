// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_THREAD_IMPL_H_
#define CONTENT_CHILD_CHILD_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sequenced_task_runner.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.
#include "ipc/ipc_platform_file.h"
#include "ipc/message_router.h"
#include "services/shell/public/cpp/shell_client.h"

namespace base {
class MessageLoop;
}  // namespace base

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
}  // namespace IPC

namespace mojo {
namespace edk {
class ScopedIPCSupport;
}  // namespace edk
}  // namespace mojo

namespace blink {
class WebFrame;
}  // namespace blink

namespace content {
class ChildMessageFilter;
class ChildDiscardableSharedMemoryManager;
class ChildGpuMemoryBufferManager;
class ChildHistogramMessageFilter;
class ChildResourceMessageFilter;
class ChildSharedBitmapManager;
class FileSystemDispatcher;
class InProcessChildThreadParams;
class NotificationDispatcher;
class PushDispatcher;
class ServiceWorkerMessageFilter;
class QuotaDispatcher;
class QuotaMessageFilter;
class ResourceDispatcher;
class ThreadSafeSender;
class WebSocketDispatcher;
class WebSocketMessageFilter;
struct RequestInfo;

// The main thread of a child process derives from this class.
class CONTENT_EXPORT ChildThreadImpl
    : public IPC::Listener,
      virtual public ChildThread,
      public NON_EXPORTED_BASE(shell::ShellClient){
 public:
  struct CONTENT_EXPORT Options;

  // Creates the thread.
  ChildThreadImpl();
  // Allow to be used for single-process mode and for in process gpu mode via
  // options.
  explicit ChildThreadImpl(const Options& options);
  // ChildProcess::main_thread() is reset after Shutdown(), and before the
  // destructor, so any subsystem that relies on ChildProcess::main_thread()
  // must be terminated before Shutdown returns. In particular, if a subsystem
  // has a thread that post tasks to ChildProcess::main_thread(), that thread
  // should be joined in Shutdown().
  ~ChildThreadImpl() override;
  virtual void Shutdown();
  void ShutdownDiscardableSharedMemoryManager();

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // ChildThread implementation:
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#endif
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  MojoShellConnection* GetMojoShellConnection() override;
  shell::InterfaceRegistry* GetInterfaceRegistry() override;
  shell::InterfaceProvider* GetRemoteInterfaces() override;

  // shell::ShellClient:
  shell::InterfaceRegistry* GetInterfaceRegistryForConnection() override;
  shell::InterfaceProvider* GetInterfaceProviderForConnection() override;

  IPC::SyncChannel* channel() { return channel_.get(); }

  IPC::MessageRouter* GetRouter();

  // Allocates a block of shared memory of the given size. Returns NULL on
  // failure.
  // Note: On posix, this requires a sync IPC to the browser process,
  // but on windows the child process directly allocates the block.
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(size_t buf_size);

  // A static variant that can be called on background threads provided
  // the |sender| passed in is safe to use on background threads.
  // |out_of_memory| is an output variable populated on failure which tells the
  // caller whether the failure was caused by an out of memory error.
  static std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t buf_size,
      IPC::Sender* sender,
      bool* out_of_memory);

  ChildSharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_.get();
  }

  ChildGpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

  ChildDiscardableSharedMemoryManager* discardable_shared_memory_manager()
      const {
    return discardable_shared_memory_manager_.get();
  }

  ResourceDispatcher* resource_dispatcher() const {
    return resource_dispatcher_.get();
  }

  WebSocketDispatcher* websocket_dispatcher() const {
    return websocket_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  QuotaDispatcher* quota_dispatcher() const {
    return quota_dispatcher_.get();
  }

  NotificationDispatcher* notification_dispatcher() const {
    return notification_dispatcher_.get();
  }

  PushDispatcher* push_dispatcher() const {
    return push_dispatcher_.get();
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

  ChildResourceMessageFilter* child_resource_message_filter() const {
    return resource_message_filter_.get();
  }

  WebSocketMessageFilter* websocket_message_filter() const {
    return websocket_message_filter_.get();
  }

  base::MessageLoop* message_loop() const { return message_loop_; }

  // Returns the one child thread. Can only be called on the main thread.
  static ChildThreadImpl* current();

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
  virtual void OnProcessBackgrounded(bool backgrounded);
  virtual void OnProcessPurgeAndSuspend();

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  bool IsInBrowserProcess() const;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner();

 private:
  class ChildThreadMessageRouter : public IPC::MessageRouter {
   public:
    // |sender| must outlive this object.
    explicit ChildThreadMessageRouter(IPC::Sender* sender);
    bool Send(IPC::Message* msg) override;

    // MessageRouter overrides.
    bool RouteMessage(const IPC::Message& msg) override;

   private:
    IPC::Sender* const sender_;
  };

  void Init(const Options& options);

  // We create the channel first without connecting it so we can add filters
  // prior to any messages being received, then connect it afterwards.
  void ConnectChannel(bool use_mojo_channel, const std::string& ipc_token);

  // IPC message handlers.
  void OnShutdown();
  void OnSetProfilerStatus(tracked_objects::ThreadData::Status status);
  void OnGetChildProfilerData(int sequence_number, int current_profiling_phase);
  void OnProfilingPhaseCompleted(int profiling_phase);
#ifdef IPC_MESSAGE_LOG_ENABLED
  void OnSetIPCLoggingEnabled(bool enable);
#endif

  void EnsureConnected();

  std::unique_ptr<mojo::edk::ScopedIPCSupport> mojo_ipc_support_;
  std::unique_ptr<MojoShellConnection> mojo_shell_connection_;
  std::unique_ptr<shell::InterfaceRegistry> interface_registry_;
  std::unique_ptr<shell::InterfaceProvider> remote_interfaces_;

  std::string channel_name_;
  std::unique_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Implements message routing functionality to the consumers of
  // ChildThreadImpl.
  ChildThreadMessageRouter router_;

  // Handles resource loads for this process.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  std::unique_ptr<WebSocketDispatcher> websocket_dispatcher_;

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  base::MessageLoop* message_loop_;

  std::unique_ptr<FileSystemDispatcher> file_system_dispatcher_;

  std::unique_ptr<QuotaDispatcher> quota_dispatcher_;

  scoped_refptr<ChildHistogramMessageFilter> histogram_message_filter_;

  scoped_refptr<ChildResourceMessageFilter> resource_message_filter_;

  scoped_refptr<ServiceWorkerMessageFilter> service_worker_message_filter_;

  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_refptr<WebSocketMessageFilter> websocket_message_filter_;

  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  scoped_refptr<PushDispatcher> push_dispatcher_;

  std::unique_ptr<ChildSharedBitmapManager> shared_bitmap_manager_;

  std::unique_ptr<ChildGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  std::unique_ptr<ChildDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  std::unique_ptr<base::PowerMonitor> power_monitor_;

  scoped_refptr<base::SequencedTaskRunner> browser_process_io_runner_;

  base::WeakPtrFactory<ChildThreadImpl> channel_connected_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildThreadImpl);
};

struct ChildThreadImpl::Options {
  Options(const Options& other);
  ~Options();

  class Builder;

  std::string channel_name;
  bool use_mojo_channel;
  scoped_refptr<base::SequencedTaskRunner> browser_process_io_runner;
  std::vector<IPC::MessageFilter*> startup_filters;
  std::string in_process_ipc_token;
  std::string in_process_application_token;

 private:
  Options();
};

class ChildThreadImpl::Options::Builder {
 public:
  Builder();

  Builder& InBrowserProcess(const InProcessChildThreadParams& params);
  Builder& UseMojoChannel(bool use_mojo_channel);
  Builder& WithChannelName(const std::string& channel_name);
  Builder& AddStartupFilter(IPC::MessageFilter* filter);

  Options Build();

 private:
  struct Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_THREAD_IMPL_H_
