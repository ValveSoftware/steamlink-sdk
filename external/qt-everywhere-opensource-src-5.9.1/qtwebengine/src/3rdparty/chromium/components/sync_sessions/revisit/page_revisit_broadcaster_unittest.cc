// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/page_revisit_broadcaster.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

class SyncPageRevisitBroadcasterTest : public ::testing::Test {
 protected:
  PageVisitObserver::TransitionType Convert(
      const ui::PageTransition conversionInput) {
    return PageRevisitBroadcaster::ConvertTransitionEnum(conversionInput);
  }

  void Check(const PageVisitObserver::TransitionType expected,
             const ui::PageTransition conversionInput) {
    EXPECT_EQ(expected, Convert(conversionInput));
  }

  void Check(const PageVisitObserver::TransitionType expected,
             const int32_t conversionInput) {
    Check(expected, ui::PageTransitionFromInt(conversionInput));
  }
};

TEST_F(SyncPageRevisitBroadcasterTest, ConvertPageInteraction) {
  Check(PageVisitObserver::kTransitionPage, ui::PAGE_TRANSITION_LINK);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_BLOCKED);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK);
  // Don't check ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, this is actually a copy
  // and paste action when combined with ui::PAGE_TRANSITION_LINK.
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_HOME_PAGE);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_END);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  Check(PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_IS_REDIRECT_MASK);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertOmniboxURL) {
  Check(PageVisitObserver::kTransitionOmniboxUrl, ui::PAGE_TRANSITION_TYPED);
  Check(PageVisitObserver::kTransitionOmniboxUrl,
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertOmniboxDefaultSearch) {
  Check(PageVisitObserver::kTransitionOmniboxDefaultSearch,
        ui::PAGE_TRANSITION_GENERATED);
  Check(PageVisitObserver::kTransitionOmniboxDefaultSearch,
        ui::PAGE_TRANSITION_GENERATED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertOmniboxTemplateSearch) {
  Check(PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD);
  Check(PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  Check(PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD_GENERATED);
  Check(PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD_GENERATED |
            ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertBookmark) {
  Check(PageVisitObserver::kTransitionBookmark,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertCopyPaste) {
  Check(PageVisitObserver::kTransitionCopyPaste,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertForwardBackward) {
  Check(PageVisitObserver::kTransitionForwardBackward,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL | ui::PAGE_TRANSITION_FORWARD_BACK);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertRestore) {
  Check(PageVisitObserver::kTransitionRestore, ui::PAGE_TRANSITION_RELOAD);
}

TEST_F(SyncPageRevisitBroadcasterTest, ConvertUnknown) {
  Check(PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  Check(PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  Check(PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace sync_sessions
