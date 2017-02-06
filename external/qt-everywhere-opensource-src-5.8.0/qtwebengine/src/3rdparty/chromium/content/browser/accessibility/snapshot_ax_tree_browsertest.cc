// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

namespace {

class AXTreeSnapshotWaiter {
 public:
  AXTreeSnapshotWaiter() : loop_runner_(new MessageLoopRunner()) {}

  void Wait() {
    loop_runner_->Run();
  }

  const ui::AXTreeUpdate& snapshot() const { return snapshot_; }

  void ReceiveSnapshot(const ui::AXTreeUpdate& snapshot) {
    snapshot_ = snapshot;
    loop_runner_->Quit();
  }

 private:
  ui::AXTreeUpdate snapshot_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSnapshotWaiter);
};

void DumpRolesAndNamesAsText(const ui::AXNode* node,
                             int indent,
                             std::string* dst) {
  for (int i = 0; i < indent; i++)
    *dst += "  ";
  *dst += ui::ToString(node->data().role);
  if (node->data().HasStringAttribute(ui::AX_ATTR_NAME))
    *dst += " '" + node->data().GetStringAttribute(ui::AX_ATTR_NAME) + "'";
  *dst += "\n";
  for (int i = 0; i < node->child_count(); ++i)
    DumpRolesAndNamesAsText(node->children()[i], indent + 1, dst);
}

}  // namespace

class SnapshotAXTreeBrowserTest : public ContentBrowserTest {
 public:
  SnapshotAXTreeBrowserTest() {}
  ~SnapshotAXTreeBrowserTest() override {}
};

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeFromWebContents) {
  GURL url("data:text/html,<button>Click</button>");
  NavigateToURL(shell(), url);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::Bind(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                 base::Unretained(&waiter)));
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, root->data().role);
  ui::AXNode* group = root->ChildAtIndex(0);
  ASSERT_EQ(ui::AX_ROLE_GROUP, group->data().role);
  ui::AXNode* button = group->ChildAtIndex(0);
  ASSERT_EQ(ui::AX_ROLE_BUTTON, button->data().role);
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeFromMultipleFrames) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(shell(), embedded_test_server()->GetURL(
      "/accessibility/snapshot/outer.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_frame = web_contents->GetFrameTree()->root();

  NavigateFrameToURL(root_frame->child_at(0), GURL("data:text/plain,Alpha"));
  NavigateFrameToURL(root_frame->child_at(1), embedded_test_server()->GetURL(
      "/accessibility/snapshot/inner.html"));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::Bind(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                 base::Unretained(&waiter)));
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  std::string dump;
  DumpRolesAndNamesAsText(root, 0, &dump);
  EXPECT_EQ(
      "rootWebArea\n"
      "  group\n"
      "    button 'Before'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        pre\n"
      "          staticText 'Alpha'\n"
      "    button 'Middle'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        group\n"
      "          button 'Inside Before'\n"
      "          iframe\n"
      "            rootWebArea\n"
      "          button 'Inside After'\n"
      "    button 'After'\n",
      dump);
}

}  // namespace content
