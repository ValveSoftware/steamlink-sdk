// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_contents_impl.h"

#include "base/message_loop/message_loop.h"
#include "blimp/client/core/blimp_contents_impl.h"
#include "blimp/client/core/public/blimp_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

const GURL kExampleURL = GURL("https://www.example.com/");
const GURL kOtherExampleURL = GURL("https://www.otherexample.com/");

class TestBlimpContentsObserver : public BlimpContentsObserver {
 public:
  TestBlimpContentsObserver() = default;
  ~TestBlimpContentsObserver() override = default;

  void OnURLUpdated(const GURL& url) override { last_url_ = url; }

  GURL GetLastURL() { return last_url_; }

 private:
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(TestBlimpContentsObserver);
};

TEST(BlimpContentsImplTest, Basic) {
  base::MessageLoop loop;
  BlimpContentsImpl blimp_contents;

  BlimpNavigationController& navigation_controller =
      blimp_contents.GetNavigationController();

  TestBlimpContentsObserver observer1;
  blimp_contents.AddObserver(&observer1);
  TestBlimpContentsObserver observer2;
  blimp_contents.AddObserver(&observer2);

  navigation_controller.LoadURL(kExampleURL);
  loop.RunUntilIdle();

  EXPECT_EQ(kExampleURL, navigation_controller.GetURL());
  EXPECT_EQ(kExampleURL, observer1.GetLastURL());
  EXPECT_EQ(kExampleURL, observer2.GetLastURL());

  // Observer should no longer receive callbacks.
  blimp_contents.RemoveObserver(&observer1);

  navigation_controller.LoadURL(kOtherExampleURL);
  loop.RunUntilIdle();

  EXPECT_EQ(kOtherExampleURL, navigation_controller.GetURL());
  EXPECT_EQ(kExampleURL, observer1.GetLastURL());
  EXPECT_EQ(kOtherExampleURL, observer2.GetLastURL());
}

}  // namespace
}  // namespace client
}  // namespace blimp
