// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// MessageFilter that handles AEC dump messages and forwards them to an
// observer.
class CONTENT_EXPORT AecDumpMessageFilter : public IPC::MessageFilter {
 public:
  class AecDumpDelegate {
   public:
    virtual void OnAecDumpFile(
        const IPC::PlatformFileForTransit& file_handle) = 0;
    virtual void OnDisableAecDump() = 0;
    virtual void OnIpcClosing() = 0;
  };

  AecDumpMessageFilter(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      PeerConnectionDependencyFactory* peerconnection_dependency_factory);

  // Getter for the one AecDumpMessageFilter object.
  static scoped_refptr<AecDumpMessageFilter> Get();

  // Adds a delegate that receives the enable and disable notifications.
  void AddDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // Removes a delegate.
  void RemoveDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // IO task runner associated with this message filter.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

 protected:
  ~AecDumpMessageFilter() override;

 private:
  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // Registers a consumer of AEC dump in the browser process. This consumer will
  // get a file handle when the AEC dump is enabled and a notification when it
  // is disabled.
  void RegisterAecDumpConsumer(int id);

  // Unregisters a consumer of AEC dump in the browser process.
  void UnregisterAecDumpConsumer(int id);

  // IPC::MessageFilter override. Called on |io_task_runner|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Accessed on |io_task_runner_|.
  void OnEnableAecDump(int id, IPC::PlatformFileForTransit file_handle);
  void OnEnableEventLog(int id, IPC::PlatformFileForTransit file_handle);
  void OnDisableAecDump();
  void OnDisableEventLog();

  // Accessed on |main_task_runner_|.
  void DoEnableAecDump(int id, IPC::PlatformFileForTransit file_handle);
  void DoEnableEventLog(int id, IPC::PlatformFileForTransit file_handle);
  void DoDisableAecDump();
  void DoDisableEventLog();
  void DoChannelClosingOnDelegates();
  int GetIdForDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // Accessed on |io_task_runner_|.
  IPC::Sender* sender_;

  // The delgates for this filter. Must only be accessed on
  // |main_task_runner_|.
  typedef std::map<int, AecDumpMessageFilter::AecDumpDelegate*> DelegateMap;
  DelegateMap delegates_;

  // Counter for generating unique IDs to delegates. Accessed on
  // |main_task_runner_|.
  int delegate_id_counter_;

  // Task runner which IPC calls are executed.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Main task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The singleton instance for this filter.
  static AecDumpMessageFilter* g_filter;

  // This pointer is used for calls to enable/disable the RTC event log.
  PeerConnectionDependencyFactory* const peerconnection_dependency_factory_;

  DISALLOW_COPY_AND_ASSIGN(AecDumpMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_
