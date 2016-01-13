// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockWebContentsDelegate : public WebContentsDelegate {
};

class WebContentsDelegateTest : public RenderViewHostImplTestHarness {
};

TEST_F(WebContentsDelegateTest, UnregisterInDestructor) {
  scoped_ptr<WebContentsImpl> contents_a(static_cast<WebContentsImpl*>(
      WebContents::Create(WebContents::CreateParams(browser_context()))));
  scoped_ptr<WebContentsImpl> contents_b(static_cast<WebContentsImpl*>(
      WebContents::Create(WebContents::CreateParams(browser_context()))));
  EXPECT_EQ(NULL, contents_a->GetDelegate());
  EXPECT_EQ(NULL, contents_b->GetDelegate());

  scoped_ptr<MockWebContentsDelegate> delegate(new MockWebContentsDelegate());

  // Setting a delegate should work correctly.
  contents_a->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // A delegate can be a delegate to multiple WebContentsImpl.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());

  // Setting the same delegate multiple times should work correctly.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());

  // Setting delegate to NULL should work correctly.
  contents_b->SetDelegate(NULL);
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // Destroying the delegate while it is still the delegate for a
  // WebContentsImpl should unregister it.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());
  delegate.reset(NULL);
  EXPECT_TRUE(contents_a->GetDelegate() == NULL);
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // Destroy the WebContentses and run the message loop to prevent leaks.
  contents_a.reset();
  contents_b.reset();
}

}  // namespace content
