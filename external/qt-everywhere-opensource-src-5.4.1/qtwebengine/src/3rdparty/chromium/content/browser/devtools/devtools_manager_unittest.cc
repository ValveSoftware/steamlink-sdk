// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace content {
namespace {

class TestDevToolsClientHost : public DevToolsClientHost {
 public:
  TestDevToolsClientHost()
      : last_sent_message(NULL),
        closed_(false) {
  }

  virtual ~TestDevToolsClientHost() {
    EXPECT_TRUE(closed_);
  }

  virtual void Close(DevToolsManager* manager) {
    EXPECT_FALSE(closed_);
    close_counter++;
    manager->ClientHostClosing(this);
    closed_ = true;
  }
  virtual void InspectedContentsClosing() OVERRIDE {
    FAIL();
  }

  virtual void DispatchOnInspectorFrontend(
      const std::string& message) OVERRIDE {
    last_sent_message = &message;
  }

  virtual void ReplacedWithAnotherClient() OVERRIDE {
  }

  static void ResetCounters() {
    close_counter = 0;
  }

  static int close_counter;

  const std::string* last_sent_message;

 private:
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};

int TestDevToolsClientHost::close_counter = 0;


class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the contents is hung.
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostImplTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    RenderViewHostImplTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
  }
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsClientHost) {
  DevToolsManager* manager = DevToolsManager::GetInstance();

  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(rvh()));
  EXPECT_FALSE(agent->IsAttached());

  TestDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(agent.get(), &client_host);
  // Test that the connection is established.
  EXPECT_TRUE(agent->IsAttached());
  EXPECT_EQ(agent, manager->GetDevToolsAgentHostFor(&client_host));
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close(manager);
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  EXPECT_FALSE(agent->IsAttached());
}

TEST_F(DevToolsManagerTest, ForwardMessageToClient) {
  DevToolsManagerImpl* manager = DevToolsManagerImpl::GetInstance();

  TestDevToolsClientHost client_host;
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetOrCreateFor(rvh()));
  manager->RegisterDevToolsClientHostFor(agent_host.get(), &client_host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  std::string m = "test message";
  agent_host = DevToolsAgentHost::GetOrCreateFor(rvh());
  manager->DispatchOnInspectorFrontend(agent_host.get(), m);
  EXPECT_TRUE(&m == client_host.last_sent_message);

  client_host.Close(manager);
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
}

TEST_F(DevToolsManagerTest, NoUnresponsiveDialogInInspectedContents) {
  TestRenderViewHost* inspected_rvh = test_rvh();
  inspected_rvh->set_render_view_created(true);
  EXPECT_FALSE(contents()->GetDelegate());
  TestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestDevToolsClientHost client_host;
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
      agent_host.get(), &client_host);

  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close devtools and check that the notification is delivered.
  client_host.Close(DevToolsManager::GetInstance());
  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->SetDelegate(NULL);
}

TEST_F(DevToolsManagerTest, ReattachOnCancelPendingNavigation) {
  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(rvh(), 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());

  TestDevToolsClientHost client_host;
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  devtools_manager->RegisterDevToolsClientHostFor(
      DevToolsAgentHost::GetOrCreateFor(rvh()).get(), &client_host);

  // Navigate to new site which should get a new RenderViewHost.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(devtools_manager->GetDevToolsAgentHostFor(&client_host),
      DevToolsAgentHost::GetOrCreateFor(pending_rvh()));

  // Interrupt pending navigation and navigate back to the original site.
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(rvh(), 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(devtools_manager->GetDevToolsAgentHostFor(&client_host),
      DevToolsAgentHost::GetOrCreateFor(rvh()));
  client_host.Close(DevToolsManager::GetInstance());
}

class TestExternalAgentDelegate: public DevToolsExternalAgentProxyDelegate {
  std::map<std::string,int> event_counter_;

  void recordEvent(const std::string& name) {
    if (event_counter_.find(name) == event_counter_.end())
      event_counter_[name] = 0;
    event_counter_[name] = event_counter_[name] + 1;
  }

  void expectEvent(int count, const std::string& name) {
    EXPECT_EQ(count, event_counter_[name]);
  }

  virtual void Attach(DevToolsExternalAgentProxy* proxy) OVERRIDE {
    recordEvent("Attach");
  };

  virtual void Detach() OVERRIDE {
    recordEvent("Detach");
  };

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    recordEvent(std::string("SendMessageToBackend.") + message);
  };

 public :
  virtual ~TestExternalAgentDelegate() {
    expectEvent(1, "Attach");
    expectEvent(1, "Detach");
    expectEvent(0, "SendMessageToBackend.message0");
    expectEvent(1, "SendMessageToBackend.message1");
    expectEvent(2, "SendMessageToBackend.message2");
  }
};

TEST_F(DevToolsManagerTest, TestExternalProxy) {
  TestExternalAgentDelegate* delegate = new TestExternalAgentDelegate();

  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::Create(delegate);
  EXPECT_EQ(agent_host, DevToolsAgentHost::GetForId(agent_host->GetId()));

  DevToolsManager* manager = DevToolsManager::GetInstance();

  TestDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(agent_host.get(), &client_host);

  manager->DispatchOnInspectorBackend(&client_host, "message1");
  manager->DispatchOnInspectorBackend(&client_host, "message2");
  manager->DispatchOnInspectorBackend(&client_host, "message2");

  client_host.Close(manager);
}

}  // namespace content
