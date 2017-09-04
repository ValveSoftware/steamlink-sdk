// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>

#include "build/build_config.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"

namespace content {

class RenderProcessHostUnitTest : public RenderViewHostTestHarness {};

// Tests that guest RenderProcessHosts are not considered suitable hosts when
// searching for RenderProcessHost.
TEST_F(RenderProcessHostUnitTest, GuestsAreNotSuitableHosts) {
  GURL test_url("http://foo.com");

  MockRenderProcessHost guest_host(browser_context());
  guest_host.set_is_for_guests_only(true);

  EXPECT_FALSE(RenderProcessHostImpl::IsSuitableHost(
      &guest_host, browser_context(), test_url));
  EXPECT_TRUE(RenderProcessHostImpl::IsSuitableHost(
      process(), browser_context(), test_url));
  EXPECT_EQ(
      process(),
      RenderProcessHost::GetExistingProcessHost(browser_context(), test_url));
}

#if !defined(OS_ANDROID)
TEST_F(RenderProcessHostUnitTest, RendererProcessLimit) {
  // This test shouldn't run with --site-per-process mode, which prohibits
  // the renderer process reuse this test explicitly exercises.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Disable any overrides.
  RenderProcessHostImpl::SetMaxRendererProcessCount(0);

  // Verify that the limit is between 1 and kMaxRendererProcessCount.
  EXPECT_GT(RenderProcessHostImpl::GetMaxRendererProcessCount(), 0u);
  EXPECT_LE(RenderProcessHostImpl::GetMaxRendererProcessCount(),
      kMaxRendererProcessCount);

  // Add dummy process hosts to saturate the limit.
  ASSERT_NE(0u, kMaxRendererProcessCount);
  ScopedVector<MockRenderProcessHost> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCount; ++i) {
    hosts.push_back(new MockRenderProcessHost(browser_context()));
  }

  // Verify that the renderer sharing will happen.
  GURL test_url("http://foo.com");
  EXPECT_TRUE(RenderProcessHostImpl::ShouldTryToUseExistingProcessHost(
        browser_context(), test_url));
}
#endif

#if defined(OS_ANDROID)
TEST_F(RenderProcessHostUnitTest, NoRendererProcessLimitOnAndroid) {
  // Disable any overrides.
  RenderProcessHostImpl::SetMaxRendererProcessCount(0);

  // Verify that by default the limit on Android returns max size_t.
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
      RenderProcessHostImpl::GetMaxRendererProcessCount());

  // Add a few dummy process hosts.
  ASSERT_NE(0u, kMaxRendererProcessCount);
  ScopedVector<MockRenderProcessHost> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCount; ++i) {
    hosts.push_back(new MockRenderProcessHost(browser_context()));
  }

  // Verify that the renderer sharing still won't happen.
  GURL test_url("http://foo.com");
  EXPECT_FALSE(RenderProcessHostImpl::ShouldTryToUseExistingProcessHost(
        browser_context(), test_url));
}
#endif

}  // namespace content
