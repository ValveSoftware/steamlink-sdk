// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/navigation_feature.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/feature/mock_navigation_feature_delegate.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

using testing::_;

namespace blimp {
namespace client {

void SendMockNavigationStateChangedMessage(BlimpMessageProcessor* processor,
                                           int tab_id,
                                           const GURL* url,
                                           const std::string* title,
                                           const bool* loading) {
  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&navigation_message, tab_id);
  navigation_message->set_type(NavigationMessage::NAVIGATION_STATE_CHANGED);
  NavigationStateChangeMessage* state =
      navigation_message->mutable_navigation_state_changed();
  if (url)
    state->set_url(url->spec());

  if (title)
    state->set_title(*title);

  if (loading)
    state->set_loading(*loading);

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

MATCHER_P2(EqualsNavigateToUrlText, tab_id, text, "") {
  return arg.target_tab_id() == tab_id &&
         arg.navigation().type() == NavigationMessage::LOAD_URL &&
         arg.navigation().load_url().url() == text;
}

MATCHER_P(EqualsNavigateForward, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
         arg.navigation().type() == NavigationMessage::GO_FORWARD;
}

MATCHER_P(EqualsNavigateBack, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
         arg.navigation().type() == NavigationMessage::GO_BACK;
}

MATCHER_P(EqualsNavigateReload, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
         arg.navigation().type() == NavigationMessage::RELOAD;
}

class NavigationFeatureTest : public testing::Test {
 public:
  NavigationFeatureTest() : out_processor_(nullptr) {}

  void SetUp() override {
    out_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_message_processor(base::WrapUnique(out_processor_));

    feature_.SetDelegate(1, &delegate1_);
    feature_.SetDelegate(2, &delegate2_);
  }

 protected:
  // This is a raw pointer to a class that is owned by the NavigationFeature.
  MockBlimpMessageProcessor* out_processor_;

  MockNavigationFeatureDelegate delegate1_;
  MockNavigationFeatureDelegate delegate2_;

  NavigationFeature feature_;
};

TEST_F(NavigationFeatureTest, DispatchesToCorrectDelegate) {
  GURL url("https://www.google.com");
  EXPECT_CALL(delegate1_, OnUrlChanged(1, url)).Times(1);
  SendMockNavigationStateChangedMessage(&feature_, 1, &url, nullptr, nullptr);

  EXPECT_CALL(delegate2_, OnUrlChanged(2, url)).Times(1);
  SendMockNavigationStateChangedMessage(&feature_, 2, &url, nullptr, nullptr);
}

TEST_F(NavigationFeatureTest, AllDelegateFieldsCalled) {
  GURL url("https://www.google.com");
  std::string title = "Google";
  bool loading = true;

  EXPECT_CALL(delegate1_, OnUrlChanged(1, url)).Times(1);
  EXPECT_CALL(delegate1_, OnTitleChanged(1, title)).Times(1);
  EXPECT_CALL(delegate1_, OnLoadingChanged(1, loading)).Times(1);
  SendMockNavigationStateChangedMessage(&feature_, 1, &url, &title, &loading);
}

TEST_F(NavigationFeatureTest, PartialDelegateFieldsCalled) {
  std::string title = "Google";
  bool loading = true;

  EXPECT_CALL(delegate1_, OnUrlChanged(_, _)).Times(0);
  EXPECT_CALL(delegate1_, OnTitleChanged(1, title)).Times(1);
  EXPECT_CALL(delegate1_, OnLoadingChanged(1, loading)).Times(1);
  SendMockNavigationStateChangedMessage(&feature_, 1, nullptr, &title,
                                        &loading);
}

TEST_F(NavigationFeatureTest, TestNavigateToUrlMessage) {
  std::string text = "http://google.com/";

  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsNavigateToUrlText(1, text), _))
      .Times(1);
  feature_.NavigateToUrlText(1, text);
}

TEST_F(NavigationFeatureTest, TestNavigateForwardMessage) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsNavigateForward(1), _))
      .Times(1);
  feature_.GoForward(1);
}

TEST_F(NavigationFeatureTest, TestNavigateBackMessage) {
  EXPECT_CALL(*out_processor_, MockableProcessMessage(EqualsNavigateBack(1), _))
      .Times(1);
  feature_.GoBack(1);
}

TEST_F(NavigationFeatureTest, TestNavigateReloadMessage) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsNavigateReload(1), _))
      .Times(1);
  feature_.Reload(1);
}

}  // namespace client
}  // namespace blimp
