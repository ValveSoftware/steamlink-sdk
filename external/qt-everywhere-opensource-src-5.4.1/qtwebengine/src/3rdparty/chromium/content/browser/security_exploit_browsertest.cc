// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

// This is a helper function for the tests which attempt to create a
// duplicate RenderViewHost or RenderWidgetHost. It tries to create two objects
// with the same process and routing ids, which causes a collision.
// It creates a couple of windows in process 1, which causes a few routing ids
// to be allocated. Then a cross-process navigation is initiated, which causes a
// new process 2 to be created and have a pending RenderViewHost for it. The
// routing id of the RenderViewHost which is target for a duplicate is set
// into |target_routing_id| and the pending RenderViewHost which is used for
// the attempt is the return value.
RenderViewHostImpl* PrepareToDuplicateHosts(Shell* shell,
                                            int* target_routing_id) {
  GURL foo("http://foo.com/files/simple_page.html");

  // Start off with initial navigation, so we get the first process allocated.
  NavigateToURL(shell, foo);

  // Open another window, so we generate some more routing ids.
  ShellAddedObserver shell2_observer;
  EXPECT_TRUE(ExecuteScript(
      shell->web_contents(), "window.open(document.URL + '#2');"));
  Shell* shell2 = shell2_observer.GetShell();

  // The new window must be in the same process, but have a new routing id.
  EXPECT_EQ(shell->web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
            shell2->web_contents()->GetRenderViewHost()->GetProcess()->GetID());
  *target_routing_id =
      shell2->web_contents()->GetRenderViewHost()->GetRoutingID();
  EXPECT_NE(*target_routing_id,
            shell->web_contents()->GetRenderViewHost()->GetRoutingID());

  // Now, simulate a link click coming from the renderer.
  GURL extension_url("https://bar.com/files/simple_page.html");
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell->web_contents());
  wc->GetFrameTree()->root()->navigator()->RequestOpenURL(
      wc->GetFrameTree()->root()->current_frame_host(), extension_url,
      Referrer(), CURRENT_TAB,
      false, true);

  // Since the navigation above requires a cross-process swap, there will be a
  // pending RenderViewHost. Ensure it exists and is in a different process
  // than the initial page.
  RenderViewHostImpl* pending_rvh =
      wc->GetRenderManagerForTesting()->pending_render_view_host();
  EXPECT_TRUE(pending_rvh != NULL);
  EXPECT_NE(shell->web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
            pending_rvh->GetProcess()->GetID());

  return pending_rvh;
}

}  // namespace


// The goal of these tests will be to "simulate" exploited renderer processes,
// which can send arbitrary IPC messages and confuse browser process internal
// state, leading to security bugs. We are trying to verify that the browser
// doesn't perform any dangerous operations in such cases.
class SecurityExploitBrowserTest : public ContentBrowserTest {
 public:
  SecurityExploitBrowserTest() {}
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ASSERT_TRUE(test_server()->Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");
  }
};

// Ensure that we kill the renderer process if we try to give it WebUI
// properties and it doesn't have enabled WebUI bindings.
IN_PROC_BROWSER_TEST_F(SecurityExploitBrowserTest, SetWebUIProperty) {
  GURL foo("http://foo.com/files/simple_page.html");

  NavigateToURL(shell(), foo);
  EXPECT_EQ(0,
      shell()->web_contents()->GetRenderViewHost()->GetEnabledBindings());

  content::RenderProcessHostWatcher terminated(
      shell()->web_contents(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  shell()->web_contents()->GetRenderViewHost()->SetWebUIProperty(
      "toolkit", "views");
  terminated.Wait();
}

// This is a test for crbug.com/312016 attempting to create duplicate
// RenderViewHosts. SetupForDuplicateHosts sets up this test case and leaves
// it in a state with pending RenderViewHost. Before the commit of the new
// pending RenderViewHost, this test case creates a new window through the new
// process.
IN_PROC_BROWSER_TEST_F(SecurityExploitBrowserTest,
                       AttemptDuplicateRenderViewHost) {
  int duplicate_routing_id = MSG_ROUTING_NONE;
  RenderViewHostImpl* pending_rvh =
      PrepareToDuplicateHosts(shell(), &duplicate_routing_id);
  EXPECT_NE(MSG_ROUTING_NONE, duplicate_routing_id);

  // Since this test executes on the UI thread and hopping threads might cause
  // different timing in the test, let's simulate a CreateNewWindow call coming
  // from the IO thread.
  ViewHostMsg_CreateWindow_Params params;
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          BrowserContext::GetStoragePartition(
              shell()->web_contents()->GetBrowserContext(),
              pending_rvh->GetSiteInstance())->GetDOMStorageContext());
  scoped_refptr<SessionStorageNamespaceImpl> session_storage(
      new SessionStorageNamespaceImpl(dom_storage_context));
  // Cause a deliberate collision in routing ids.
  int main_frame_routing_id = duplicate_routing_id + 1;
  pending_rvh->CreateNewWindow(
      duplicate_routing_id, main_frame_routing_id, params, session_storage);

  // If the above operation doesn't cause a crash, the test has succeeded!
}

// This is a test for crbug.com/312016. It tries to create two RenderWidgetHosts
// with the same process and routing ids, which causes a collision. It is almost
// identical to the AttemptDuplicateRenderViewHost test case.
IN_PROC_BROWSER_TEST_F(SecurityExploitBrowserTest,
                       AttemptDuplicateRenderWidgetHost) {
  int duplicate_routing_id = MSG_ROUTING_NONE;
  RenderViewHostImpl* pending_rvh =
      PrepareToDuplicateHosts(shell(), &duplicate_routing_id);
  EXPECT_NE(MSG_ROUTING_NONE, duplicate_routing_id);

  // Since this test executes on the UI thread and hopping threads might cause
  // different timing in the test, let's simulate a CreateNewWidget call coming
  // from the IO thread.  Use the existing window routing id to cause a
  // deliberate collision.
  pending_rvh->CreateNewWidget(duplicate_routing_id, blink::WebPopupTypeSelect);

  // If the above operation doesn't crash, the test has succeeded!
}

}  // namespace content
