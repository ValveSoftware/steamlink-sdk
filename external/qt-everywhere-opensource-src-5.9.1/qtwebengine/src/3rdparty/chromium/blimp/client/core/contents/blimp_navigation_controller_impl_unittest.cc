// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"
#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"
#include "blimp/client/core/contents/fake_navigation_feature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {
namespace {

const int kDummyBlimpContentsId = 0;
const GURL kExampleURL = GURL("https://www.example.com/");

class MockBlimpNavigationControllerDelegate
    : public BlimpNavigationControllerDelegate {
 public:
  MockBlimpNavigationControllerDelegate() = default;
  ~MockBlimpNavigationControllerDelegate() override = default;

  MOCK_METHOD0(OnNavigationStateChanged, void());
  MOCK_METHOD1(OnLoadingStateChanged, void(bool loading));
  MOCK_METHOD1(OnPageLoadingStateChanged, void(bool loading));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlimpNavigationControllerDelegate);
};

TEST(BlimpNavigationControllerImplTest, BackForwardNavigation) {
  base::MessageLoop loop;

  testing::StrictMock<MockBlimpNavigationControllerDelegate> delegate;
  testing::StrictMock<FakeNavigationFeature> feature;
  BlimpNavigationControllerImpl navigation_controller(kDummyBlimpContentsId,
                                                      &delegate, &feature);
  feature.SetDelegate(1, &navigation_controller);

  EXPECT_CALL(delegate, OnNavigationStateChanged());

  navigation_controller.LoadURL(kExampleURL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kExampleURL, navigation_controller.GetURL());

  EXPECT_CALL(feature, GoBack(_));
  EXPECT_CALL(feature, GoForward(_));
  EXPECT_CALL(feature, Reload(_));

  navigation_controller.GoBack();
  navigation_controller.GoForward();
  navigation_controller.Reload();

  base::RunLoop().RunUntilIdle();
}

TEST(BlimpNavigationControllerImplTest, Loading) {
  testing::InSequence s;
  base::MessageLoop loop;

  testing::StrictMock<MockBlimpNavigationControllerDelegate> delegate;
  testing::StrictMock<FakeNavigationFeature> feature;
  BlimpNavigationControllerImpl navigation_controller(kDummyBlimpContentsId,
                                                      &delegate, &feature);
  feature.SetDelegate(1, &navigation_controller);

  EXPECT_CALL(delegate, OnNavigationStateChanged());
  EXPECT_CALL(delegate, OnLoadingStateChanged(true));
  EXPECT_CALL(delegate, OnNavigationStateChanged());
  EXPECT_CALL(delegate, OnLoadingStateChanged(false));

  NavigationFeature::NavigationFeatureDelegate* feature_delegate =
      static_cast<NavigationFeature::NavigationFeatureDelegate*>(
          &navigation_controller);

  feature_delegate->OnLoadingChanged(1, true);
  feature_delegate->OnLoadingChanged(1, false);
}

}  // namespace
}  // namespace client
}  // namespace blimp
