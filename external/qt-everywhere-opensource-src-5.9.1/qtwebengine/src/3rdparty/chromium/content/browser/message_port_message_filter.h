// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/message_port_delegate.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

struct FrameMsg_PostMessage_Params;

namespace content {

// Filter for MessagePort related IPC messages (creating and destroying a
// MessagePort, sending a message via a MessagePort etc).
class CONTENT_EXPORT MessagePortMessageFilter
    : public MessagePortDelegate,
      public BrowserMessageFilter {
 public:
  typedef base::Callback<int(void)> NextRoutingIDCallback;

  // |next_routing_id| is owned by this object.  It can be used up until
  // OnChannelClosing.
  explicit MessagePortMessageFilter(const NextRoutingIDCallback& callback);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  int GetNextRoutingID();

  // MessagePortDelegate implementation.
  void SendMessage(
      int route_id,
      const base::string16& message,
      const std::vector<int>& sent_message_ports) override;
  void SendMessagesAreQueued(int route_id) override;

  // Updates message ports registered for |message_ports| and returns
  // new routing IDs for the updated ports via |new_routing_ids|.
  void UpdateMessagePortsWithNewRoutes(
      const std::vector<int>& message_ports,
      std::vector<int>* new_routing_ids);

  void RouteMessageEventWithMessagePorts(
      int routing_id,
      const FrameMsg_PostMessage_Params& params);

 protected:
  // This is protected, so we can define sub classes for testing.
  ~MessagePortMessageFilter() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<MessagePortMessageFilter>;

  // Message handlers.
  void OnCreateMessagePort(int* route_id, int* message_port_id);

  // This is guaranteed to be valid until OnChannelClosing is invoked, and it's
  // not used after.
  NextRoutingIDCallback next_routing_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MessagePortMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_
