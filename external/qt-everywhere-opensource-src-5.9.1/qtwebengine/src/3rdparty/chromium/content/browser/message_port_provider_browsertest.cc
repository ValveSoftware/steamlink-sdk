// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/message_port_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_delegate.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

// This test verifies the functionality of the Message Port Provider API.

// A mock class for testing message port provider.
class MockMessagePortDelegate : public MessagePortDelegate {
 public:
  // A container to hold received messages
  struct Message {
    int route_id;  // the routing id of the target port
    base::string16 data;  // the message data
    std::vector<int> sent_ports;  // any transferred ports
  };

  typedef std::vector<Message> Messages;

  MockMessagePortDelegate() { }
  ~MockMessagePortDelegate() override { }

  // MessagePortDelegate implementation
  void SendMessage(
      int route_id,
      const base::string16& message,
      const std::vector<int>& sent_message_ports) override {
    Message m;
    m.route_id = route_id;
    m.data = message;
    m.sent_ports = sent_message_ports;
    messages_.push_back(m);
  }

  void SendMessagesAreQueued(int route_id) override { }

  const Messages& getReceivedMessages() {
    return messages_;
  }
 private:
  Messages messages_;

  DISALLOW_COPY_AND_ASSIGN(MockMessagePortDelegate);
};


class MessagePortProviderBrowserTest : public ContentBrowserTest {
};

// Verify that messages can be posted to main frame.
IN_PROC_BROWSER_TEST_F(MessagePortProviderBrowserTest, PostMessage) {
  const std::string data =
      "<!DOCTYPE html><html><body>"
      "    <script type=\"text/javascript\">"
      "        onmessage = function (e) { document.title = e.data; }"
      "   </script>"
      "</body></html>";
  const base::string16 target_origin(base::UTF8ToUTF16("http://baseurl"));
  const GURL base_url(target_origin);
  const GURL history_url;
  // Load data. Blocks until it is done.
  content::LoadDataWithBaseURL(shell(), history_url, data, base_url);
  const base::string16 source_origin(base::UTF8ToUTF16("source"));
  const base::string16 message(base::UTF8ToUTF16("success"));
  const std::vector<int> ports;
  content::TitleWatcher title_watcher(shell()->web_contents(), message);
  MessagePortProvider::PostMessageToFrame(shell()->web_contents(),
                                          source_origin,
                                          target_origin,
                                          message,
                                          ports);
  EXPECT_EQ(message, title_watcher.WaitAndGetTitle());
}

}  // namespace content
