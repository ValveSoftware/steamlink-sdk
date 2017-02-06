// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/stream_info.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class TestStreamHandle : public content::StreamHandle {
 public:
  TestStreamHandle() : url_("stream://url") {}

  ~TestStreamHandle() override {
    while (!close_callbacks_.empty()) {
      close_callbacks_.front().Run();
      close_callbacks_.pop();
    }
  }

  const GURL& GetURL() override { return url_; }

  void AddCloseListener(const base::Closure& callback) override {
    close_callbacks_.push(callback);
  }

 private:
  GURL url_;
  std::queue<base::Closure> close_callbacks_;
};

class MimeHandlerServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<content::StreamInfo> stream_info(new content::StreamInfo);
    stream_info->handle = base::WrapUnique(new TestStreamHandle);
    stream_info->mime_type = "test/unit";
    stream_info->original_url = GURL("test://extensions_unittests");
    stream_container_.reset(
        new StreamContainer(std::move(stream_info), 1, true, GURL(), ""));
    service_.reset(new MimeHandlerServiceImpl(stream_container_->GetWeakPtr(),
                                              mojo::GetProxy(&service_ptr_)));
  }
  void TearDown() override {
    service_.reset();
    stream_container_.reset();
  }

  void AbortCallback() { abort_called_ = true; }
  void GetStreamInfoCallback(mime_handler::StreamInfoPtr stream_info) {
    stream_info_ = std::move(stream_info);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<StreamContainer> stream_container_;
  mime_handler::MimeHandlerServicePtr service_ptr_;
  std::unique_ptr<mime_handler::MimeHandlerService> service_;
  bool abort_called_ = false;
  mime_handler::StreamInfoPtr stream_info_;
};

TEST_F(MimeHandlerServiceImplTest, Abort) {
  service_->AbortStream(base::Bind(&MimeHandlerServiceImplTest::AbortCallback,
                                   base::Unretained(this)));
  EXPECT_TRUE(abort_called_);

  abort_called_ = false;
  service_->AbortStream(base::Bind(&MimeHandlerServiceImplTest::AbortCallback,
                                   base::Unretained(this)));
  EXPECT_TRUE(abort_called_);

  stream_container_.reset();
  abort_called_ = false;
  service_->AbortStream(base::Bind(&MimeHandlerServiceImplTest::AbortCallback,
                                   base::Unretained(this)));
  EXPECT_TRUE(abort_called_);
}

TEST_F(MimeHandlerServiceImplTest, GetStreamInfo) {
  service_->GetStreamInfo(
      base::Bind(&MimeHandlerServiceImplTest::GetStreamInfoCallback,
                 base::Unretained(this)));
  ASSERT_TRUE(stream_info_);
  EXPECT_TRUE(stream_info_->embedded);
  EXPECT_EQ(1, stream_info_->tab_id);
  EXPECT_EQ("test/unit", stream_info_->mime_type);
  EXPECT_EQ("test://extensions_unittests", stream_info_->original_url);
  EXPECT_EQ("stream://url", stream_info_->stream_url);

  service_->AbortStream(base::Bind(&MimeHandlerServiceImplTest::AbortCallback,
                                   base::Unretained(this)));
  EXPECT_TRUE(abort_called_);
  service_->GetStreamInfo(
      base::Bind(&MimeHandlerServiceImplTest::GetStreamInfoCallback,
                 base::Unretained(this)));
  ASSERT_FALSE(stream_info_);

  stream_container_.reset();
  service_->GetStreamInfo(
      base::Bind(&MimeHandlerServiceImplTest::GetStreamInfoCallback,
                 base::Unretained(this)));
  ASSERT_FALSE(stream_info_);
}

}  // namespace extensions
