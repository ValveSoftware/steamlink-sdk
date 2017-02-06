// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

class MessagePortDelegate;
class WebContents;

// An interface consisting of methods that can be called to use Message ports.
class CONTENT_EXPORT MessagePortProvider {
 public:
  // Posts a MessageEvent to the main frame using the given source and target
  // origins and data. The caller may also provide any message port ids as
  // part of the message.
  // See https://html.spec.whatwg.org/multipage/comms.html#messageevent for
  // further information on message events.
  // Should be called on UI thread.
  static void PostMessageToFrame(
      WebContents* web_contents,
      const base::string16& source_origin,
      const base::string16& target_origin,
      const base::string16& data,
      const std::vector<int>& ports);

  // Creates a message channel and provide the ids of the message ports that are
  // associated with this message channel.
  // See https://html.spec.whatwg.org/multipage/comms.html#messagechannel
  // Should be called on IO thread.
  // The message ports that are created will have their routing id numbers equal
  // to the message port numbers.
  static void CreateMessageChannel(MessagePortDelegate* delegate,
                                   int* port1,
                                   int* port2);

  // Posts a MessageEvent to a message port associated with a message channel.
  // Should be called on IO thread.
  static void PostMessageToPort(
      int sender_port_id,
      const base::string16& message,
      const std::vector<int>& sent_ports);

  // Close the message port. Should be called on IO thread.
  static void ClosePort(int message_port_id);

  // Queue up all the messages for this message port until ReleaseMessages
  // is called. Should be called on IO thread.
  static void HoldMessages(int message_port_id);

  // Release any queued messages as a result of HoldMessages. Should be
  // called on IO thread.
  static void ReleaseMessages(int message_port_id);

  // Cleanup the message ports that belong to the closing delegate.
  // Should be called on IO thread.
  static void OnMessagePortDelegateClosing(MessagePortDelegate * delegate);

  // Update message port information when the message port is transferred
  // from a different process. The updated message ports will have their
  // routing numbers equal to the message port numbers.
  // Should be called on IO thread.
  static void UpdateMessagePort(int message_port_id,
                                MessagePortDelegate* delegate);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MessagePortProvider);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
