// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"

#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"

namespace dom_distiller {

class WebContentsMainFrameObserverTest
    : public content::RenderViewHostTestHarness {
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
        web_contents());
    main_frame_observer_ =
        WebContentsMainFrameObserver::FromWebContents(web_contents());
    ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
  }

 protected:
  WebContentsMainFrameObserver* main_frame_observer_;  // weak
};

TEST_F(WebContentsMainFrameObserverTest, ListensForMainFrameNavigation) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_TRUE(main_frame_observer_->is_initialized());
  ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());

  main_frame_observer_->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, IgnoresChildFrameNavigation) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = false;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(main_frame_observer_->is_initialized());
  ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, IgnoresInPageNavigation) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = true;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(main_frame_observer_->is_initialized());
  ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest,
       IgnoresInPageNavigationUnlessMainFrameLoads) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = true;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(main_frame_observer_->is_initialized());
  ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Even if we didn't acknowledge an in_page navigation, if the main frame
  // loads, consider a load complete.
  main_frame_observer_->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, ResetOnPageNavigation) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  main_frame_observer_->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Another navigation should result in waiting for a page load.
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_TRUE(main_frame_observer_->is_initialized());
  ASSERT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, DoesNotResetOnInPageNavigation) {
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  main_frame_observer_->DidNavigateMainFrame(details, params);
  main_frame_observer_->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Navigating withing the page should not result in waiting for a page load.
  details.is_in_page = true;
  main_frame_observer_->DidNavigateMainFrame(details, params);
  ASSERT_TRUE(main_frame_observer_->is_initialized());
  ASSERT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

}  // namespace dom_distiller
