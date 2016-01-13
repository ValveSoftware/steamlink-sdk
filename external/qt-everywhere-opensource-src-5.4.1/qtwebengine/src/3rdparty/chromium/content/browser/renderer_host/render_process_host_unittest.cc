// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_view_host.h"

namespace content {

class RenderProcessHostUnitTest : public RenderViewHostTestHarness {};

// Tests that guest RenderProcessHosts are not considered suitable hosts when
// searching for RenderProcessHost.
TEST_F(RenderProcessHostUnitTest, GuestsAreNotSuitableHosts) {
  GURL test_url("http://foo.com");

  MockRenderProcessHost guest_host(browser_context());
  guest_host.set_is_isolated_guest(true);

  EXPECT_FALSE(RenderProcessHostImpl::IsSuitableHost(
      &guest_host, browser_context(), test_url));
  EXPECT_TRUE(RenderProcessHostImpl::IsSuitableHost(
      process(), browser_context(), test_url));
  EXPECT_EQ(
      process(),
      RenderProcessHost::GetExistingProcessHost(browser_context(), test_url));
}

}  // namespace content
