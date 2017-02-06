// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_navigation_controller_impl.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/blimp_navigation_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

const GURL kExampleURL = GURL("https://www.example.com/");

class TestBlimpNavigationControllerDelegate
    : public BlimpNavigationControllerDelegate {
 public:
  TestBlimpNavigationControllerDelegate() = default;
  ~TestBlimpNavigationControllerDelegate() override = default;

  void NotifyURLLoaded(const GURL& url) override { last_loaded_url_ = url; }

  GURL GetLastLoadedURL() { return last_loaded_url_; }

 private:
  GURL last_loaded_url_;

  DISALLOW_COPY_AND_ASSIGN(TestBlimpNavigationControllerDelegate);
};

TEST(BlimpNavigationControllerImplTest, Basic) {
  base::MessageLoop loop;

  TestBlimpNavigationControllerDelegate delegate;
  BlimpNavigationControllerImpl navigation_controller(&delegate);

  navigation_controller.LoadURL(kExampleURL);
  EXPECT_EQ(kExampleURL, navigation_controller.GetURL());

  loop.RunUntilIdle();
  EXPECT_EQ(kExampleURL, delegate.GetLastLoadedURL());
}

}  // namespace
}  // namespace client
}  // namespace blimp
