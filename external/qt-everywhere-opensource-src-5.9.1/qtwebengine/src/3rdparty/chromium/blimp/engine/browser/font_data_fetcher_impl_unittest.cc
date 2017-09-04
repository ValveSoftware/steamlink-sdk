// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/font_data_fetcher_impl.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace engine {

class FontDataFetcherImplTest : public testing::Test {
 public:
  FontDataFetcherImplTest() {}
  ~FontDataFetcherImplTest() override {}

  void SetUp() override {
    font_fetcher_ = base::MakeUnique<FontDataFetcherImpl>(
        base::ThreadTaskRunnerHandle::Get());
  }

  void OnFontResponse(std::unique_ptr<SkStream> stream) {
    MockFontResponseCallback(stream);
  }

  MOCK_METHOD1(MockFontResponseCallback, void(std::unique_ptr<SkStream> &));

 protected:
  // A message loop to emulate asynchronous behavior.
  base::MessageLoop message_loop_;
  std::unique_ptr<FontDataFetcher> font_fetcher_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

TEST_F(FontDataFetcherImplTest, FetchFontStream) {
  EXPECT_CALL(*this, MockFontResponseCallback(_)).Times(1);
  font_fetcher_->FetchFontStream(
      "fake font hash",
      base::Bind(&FontDataFetcherImplTest::OnFontResponse,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
}

}  // namespace engine
}  // namespace blimp
