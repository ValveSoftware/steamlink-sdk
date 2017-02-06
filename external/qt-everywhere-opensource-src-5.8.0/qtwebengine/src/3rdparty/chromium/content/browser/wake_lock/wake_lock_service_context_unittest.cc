// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_context.h"

#include <memory>

#include "base/process/kill.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"

namespace content {

class RenderFrameHost;

class WakeLockServiceContextTest : public RenderViewHostTestHarness {
 protected:
  void RequestWakeLock(RenderFrameHost* rfh) {
    GetWakeLockServiceContext()->RequestWakeLock(rfh->GetProcess()->GetID(),
                                                 rfh->GetRoutingID());
  }

  void CancelWakeLock(RenderFrameHost* rfh) {
    GetWakeLockServiceContext()->CancelWakeLock(rfh->GetProcess()->GetID(),
                                                rfh->GetRoutingID());
  }

  WakeLockServiceContext* GetWakeLockServiceContext() {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(web_contents());
    return web_contents_impl->GetWakeLockServiceContext();
  }

  bool HasWakeLock() {
    return GetWakeLockServiceContext()->HasWakeLockForTests();
  }
};

TEST_F(WakeLockServiceContextTest, NoLockInitially) {
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, LockUnlock) {
  ASSERT_TRUE(GetWakeLockServiceContext());
  ASSERT_TRUE(web_contents());
  ASSERT_TRUE(main_rfh());

  // Request wake lock for main frame.
  RequestWakeLock(main_rfh());

  // Should set the blocker.
  EXPECT_TRUE(HasWakeLock());

  // Remove wake lock request for main frame.
  CancelWakeLock(main_rfh());

  // Should remove the blocker.
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, RenderFrameDeleted) {
  ASSERT_TRUE(GetWakeLockServiceContext());
  ASSERT_TRUE(web_contents());
  ASSERT_TRUE(main_rfh());

  // Request wake lock for main frame.
  RequestWakeLock(main_rfh());

  // Should set the blocker.
  EXPECT_TRUE(HasWakeLock());

  // Simulate render frame deletion.
  GetWakeLockServiceContext()->RenderFrameDeleted(main_rfh());

  // Should remove the blocker.
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, NoLockForBogusFrameId) {
  ASSERT_TRUE(GetWakeLockServiceContext());
  ASSERT_TRUE(web_contents());

  // Request wake lock for non-existent render frame id.
  int non_existent_render_frame_id =
      main_rfh()->GetProcess()->GetNextRoutingID();
  GetWakeLockServiceContext()->RequestWakeLock(
      main_rfh()->GetProcess()->GetID(), non_existent_render_frame_id);

  // Should not set the blocker.
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace content
