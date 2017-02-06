// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_system_url_request_context_getter.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

namespace {
class BlimpSystemURLRequestContextGetterTest : public testing::Test {
 public:
  BlimpSystemURLRequestContextGetterTest()
      : system_url_request_context_getter_(
            new BlimpSystemURLRequestContextGetter()) {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<BlimpSystemURLRequestContextGetter>
      system_url_request_context_getter_;
};

TEST_F(BlimpSystemURLRequestContextGetterTest, GetURLRequestContext) {
  ASSERT_NE(nullptr,
            system_url_request_context_getter_->GetURLRequestContext());
}

}  // namespace

}  // namespace engine
}  // namespace blimp
