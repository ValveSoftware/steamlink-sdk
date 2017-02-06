// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_
#define CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class ChildThread;

// This is thread safe.
class WebMessagePortChannelImpl
    : public blink::WebMessagePortChannel,
      public IPC::Listener,
      public base::RefCountedThreadSafe<WebMessagePortChannelImpl> {
 public:
  explicit WebMessagePortChannelImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);
  WebMessagePortChannelImpl(
      int route_id,
      int port_id,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);

  static void CreatePair(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      blink::WebMessagePortChannel** channel1,
      blink::WebMessagePortChannel** channel2);

  // Extracts port IDs for passing on to the browser process, and queues any
  // received messages.
  static std::vector<int> ExtractMessagePortIDs(
      std::unique_ptr<blink::WebMessagePortChannelArray> channels);

  // Extracts port IDs for passing on to the browser process, and queues any
  // received messages.
  static std::vector<int> ExtractMessagePortIDs(
      const blink::WebMessagePortChannelArray& channels);

  // Extracts port IDs for passing on to the browser process, but doesn't
  // send a separate IPC to the browser to initiate queueing messages. Instead
  // calling code is responsible for initiating the queueing in the browser
  // process. This is useful when transfering ports over an IPC channel that
  // does not share ordering guarentees with regular IPC.
  static std::vector<int>
  ExtractMessagePortIDsWithoutQueueing(
      std::unique_ptr<blink::WebMessagePortChannelArray> channels);

  // Creates WebMessagePortChannelImpl instances for port IDs passed in from the
  // browser process.
  static blink::WebMessagePortChannelArray CreatePorts(
      const std::vector<int>& message_ports,
      const std::vector<int>& new_routing_ids,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);

  // Queues received and incoming messages until there are no more in-flight
  // messages, then sends all of them to the browser process.
  void QueueMessages();
  int message_port_id() const { return message_port_id_; }

 private:
  friend class base::RefCountedThreadSafe<WebMessagePortChannelImpl>;
  ~WebMessagePortChannelImpl() override;

  // WebMessagePortChannel implementation.
  void setClient(blink::WebMessagePortChannelClient* client) override;
  void destroy() override;
  void postMessage(const blink::WebString& message,
                   blink::WebMessagePortChannelArray* channels_ptr) override;
  bool tryGetMessage(blink::WebString* message,
                     blink::WebMessagePortChannelArray& channels) override;

  void Init();
  void Entangle(scoped_refptr<WebMessagePortChannelImpl> channel);
  void Send(IPC::Message* message);
  void SendPostMessage(
      const base::string16& message,
      std::unique_ptr<blink::WebMessagePortChannelArray> channels);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnMessage(const base::string16& message,
                 const std::vector<int>& sent_message_ports,
                 const std::vector<int>& new_routing_ids);
  void OnMessagesQueued();

  struct Message {
    Message();
    Message(const Message& other);
    ~Message();

    base::string16 message;
    blink::WebMessagePortChannelArray ports;
  };

  typedef std::queue<Message> MessageQueue;
  MessageQueue message_queue_;

  blink::WebMessagePortChannelClient* client_;
  base::Lock lock_;  // Locks access to above.

  int route_id_;  // The routing id for this object.
  int message_port_id_;  // A globally unique identifier for this message port.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebMessagePortChannelImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_
