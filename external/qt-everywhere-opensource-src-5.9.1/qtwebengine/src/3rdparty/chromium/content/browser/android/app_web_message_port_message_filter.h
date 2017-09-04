// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/message_port_delegate.h"

namespace content {

// Filter for Aw specific MessagePort related IPC messages (creating and
// destroying a MessagePort, sending a message via a MessagePort etc).
class AppWebMessagePortMessageFilter : public content::BrowserMessageFilter,
                                       public content::MessagePortDelegate {
 public:
  explicit AppWebMessagePortMessageFilter(int route_id);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  void SendAppToWebMessage(int msg_port_route_id,
                           const base::string16& message,
                           const std::vector<int>& sent_message_port_ids);
  void SendClosePortMessage(int message_port_id);

  // MessagePortDelegate implementation.
  void SendMessage(int msg_port_route_id,
                   const base::string16& message,
                   const std::vector<int>& sent_message_ports) override;
  void SendMessagesAreQueued(int route_id) override;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<AppWebMessagePortMessageFilter>;

  void OnConvertedAppToWebMessage(
      int msg_port_id,
      const base::string16& message,
      const std::vector<int>& sent_message_port_ids);
  void OnClosePortAck(int message_port_id);

  int route_id_;

  ~AppWebMessagePortMessageFilter() override;

  DISALLOW_COPY_AND_ASSIGN(AppWebMessagePortMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_MESSAGE_FILTER_H_
