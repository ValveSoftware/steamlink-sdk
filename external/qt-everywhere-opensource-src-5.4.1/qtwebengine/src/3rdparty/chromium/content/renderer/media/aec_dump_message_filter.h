// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_filter.h"

namespace base {
class MessageLoopProxy;
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
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      const scoped_refptr<base::MessageLoopProxy>& main_message_loop);

  // Getter for the one AecDumpMessageFilter object.
  static scoped_refptr<AecDumpMessageFilter> Get();

  // Adds a delegate that receives the enable and disable notifications.
  void AddDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // Removes a delegate.
  void RemoveDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  virtual ~AecDumpMessageFilter();

 private:
  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // Registers a consumer of AEC dump in the browser process. This consumer will
  // get a file handle when the AEC dump is enabled and a notification when it
  // is disabled.
  void RegisterAecDumpConsumer(int id);

  // Unregisters a consumer of AEC dump in the browser process.
  void UnregisterAecDumpConsumer(int id);

  // IPC::MessageFilter override. Called on |io_message_loop|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Accessed on |io_message_loop|.
  void OnEnableAecDump(int id, IPC::PlatformFileForTransit file_handle);
  void OnDisableAecDump();

  // Accessed on |main_message_loop_|.
  void DoEnableAecDump(int id, IPC::PlatformFileForTransit file_handle);
  void DoDisableAecDump();
  void DoChannelClosingOnDelegates();
  int GetIdForDelegate(AecDumpMessageFilter::AecDumpDelegate* delegate);

  // Accessed on |io_message_loop_|.
  IPC::Sender* sender_;

  // The delgates for this filter. Must only be accessed on
  // |main_message_loop_|.
  typedef std::map<int, AecDumpMessageFilter::AecDumpDelegate*> DelegateMap;
  DelegateMap delegates_;

  // Counter for generating unique IDs to delegates. Accessed on
  // |main_message_loop_|.
  int delegate_id_counter_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Main message loop.
  const scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  // The singleton instance for this filter.
  static AecDumpMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AecDumpMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AEC_DUMP_MESSAGE_FILTER_H_
