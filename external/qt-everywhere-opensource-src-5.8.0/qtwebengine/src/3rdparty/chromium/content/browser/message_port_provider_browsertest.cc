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

namespace {

void VerifyCreateChannelOnIOThread(base::WaitableEvent* event) {

  const base::char16 MESSAGE1[] = { 0x1000, 0 };
  const base::char16 MESSAGE2[] = { 0x1001, 0 };

  MockMessagePortDelegate delegate;
  int port1;
  int port2;

  MessagePortProvider::CreateMessageChannel(&delegate, &port1, &port2);
  MessagePortService* service = MessagePortService::GetInstance();
  // Send a message to port1 transferring no ports.
  std::vector<int> sent_ports;
  service->PostMessage(port1, base::string16(MESSAGE1), sent_ports);
  // Verify that message is received
  const MockMessagePortDelegate::Messages& received =
      delegate.getReceivedMessages();
  EXPECT_EQ(received.size(), 1u);
  // Verify that message sent to port1 is received by entangled port, which is
  // port2.
  EXPECT_EQ(received[0].route_id, port2);
  EXPECT_EQ(received[0].data, MESSAGE1);
  EXPECT_EQ(received[0].sent_ports.size(), 0u);

  // Create a new channel, and transfer one of its ports to port2, making sure
  // the transferred port is received.
  int port3;
  int port4;
  MessagePortProvider::CreateMessageChannel(&delegate, &port3, &port4);
  sent_ports.push_back(port3);
  service->PostMessage(port1, base::string16(MESSAGE2), sent_ports);
  EXPECT_EQ(received.size(), 2u);
  EXPECT_EQ(received[1].route_id, port2);
  EXPECT_EQ(received[1].data, MESSAGE2);
  EXPECT_EQ(received[1].sent_ports.size(), 1u);
  EXPECT_EQ(received[1].sent_ports[0], port3);

  event->Signal();
}

}  // namespace

// Verify that a message channel can be created and used for exchanging
// messages.
IN_PROC_BROWSER_TEST_F(MessagePortProviderBrowserTest, CreateChannel) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VerifyCreateChannelOnIOThread, &event));
  event.Wait();
}

}  // namespace content
