// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class SitePerProcessDevToolsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessDevToolsBrowserTest() {}
};

class TestClient: public DevToolsAgentHostClient {
 public:
  TestClient() : closed_(false), waiting_for_reply_(false) {}
  ~TestClient() override {}

  bool closed() { return closed_; }

  void DispatchProtocolMessage(
      DevToolsAgentHost* agent_host,
      const std::string& message) override {
    if (waiting_for_reply_) {
      waiting_for_reply_ = false;
      base::MessageLoop::current()->QuitNow();
    }
  }

  void AgentHostClosed(
      DevToolsAgentHost* agent_host,
      bool replaced_with_another_client) override {
    closed_ = true;
  }

  void WaitForReply() {
    waiting_for_reply_ = true;
    base::MessageLoop::current()->Run();
  }

 private:
  bool closed_;
  bool waiting_for_reply_;
};

// Fails on Android, http://crbug.com/464993.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteIframeAgentHost DISABLED_CrossSiteIframeAgentHost
#else
#define MAYBE_CrossSiteIframeAgentHost CrossSiteIframeAgentHost
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest,
                       MAYBE_CrossSiteIframeAgentHost) {
  DevToolsAgentHost::List list;
  host_resolver()->AddRule("*", "127.0.0.1");
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_EQ(DevToolsAgentHost::TYPE_FRAME, list[1]->GetType());
  EXPECT_EQ(cross_site_url.spec(), list[1]->GetURL().spec());

  // Attaching to both agent hosts.
  scoped_refptr<DevToolsAgentHost> child_host = list[1];
  TestClient child_client;
  child_host->AttachClient(&child_client);
  scoped_refptr<DevToolsAgentHost> parent_host = list[0];
  TestClient parent_client;
  parent_host->AttachClient(&parent_client);

  // Send message to parent and child frames and get result back.
  char message[] = "{\"id\": 0, \"method\": \"incorrect.method\"}";
  child_host->DispatchProtocolMessage(&child_client, message);
  child_client.WaitForReply();
  parent_host->DispatchProtocolMessage(&parent_client, message);
  parent_client.WaitForReply();

  // Load back same-site page into iframe.
  NavigateFrameToURL(root->child_at(0), http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_TRUE(child_client.closed());
  child_host->DetachClient(&child_client);
  child_host = nullptr;
  EXPECT_FALSE(parent_client.closed());
  parent_host->DetachClient(&parent_client);
  parent_host = nullptr;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest, AgentHostForFrames) {
  host_resolver()->AddRule("*", "127.0.0.1");
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  scoped_refptr<DevToolsAgentHost> page_agent =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  scoped_refptr<DevToolsAgentHost> main_frame_agent =
      DevToolsAgentHost::GetOrCreateFor(root->current_frame_host());
  EXPECT_EQ(page_agent.get(), main_frame_agent.get());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);

  scoped_refptr<DevToolsAgentHost> child_frame_agent =
      DevToolsAgentHost::GetOrCreateFor(child->current_frame_host());
  EXPECT_EQ(page_agent.get(), child_frame_agent.get());

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  child_frame_agent =
      DevToolsAgentHost::GetOrCreateFor(child->current_frame_host());
  EXPECT_NE(page_agent.get(), child_frame_agent.get());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest,
    AgentHostForPageEqualsOneForMainFrame) {
  host_resolver()->AddRule("*", "127.0.0.1");
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(child, cross_site_url);

  // First ask for child frame, then for main frame.
  scoped_refptr<DevToolsAgentHost> child_frame_agent =
      DevToolsAgentHost::GetOrCreateFor(child->current_frame_host());
  scoped_refptr<DevToolsAgentHost> main_frame_agent =
      DevToolsAgentHost::GetOrCreateFor(root->current_frame_host());
  EXPECT_NE(main_frame_agent.get(), child_frame_agent.get());

  // Agent for web contents should be the the main frame's one.
  scoped_refptr<DevToolsAgentHost> page_agent =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  EXPECT_EQ(page_agent.get(), main_frame_agent.get());
}

}  // namespace content
