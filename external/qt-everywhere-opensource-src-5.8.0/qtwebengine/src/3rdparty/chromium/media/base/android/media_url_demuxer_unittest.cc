// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/run_loop.h"
#include "media/base/android/media_url_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MediaUrlDemuxerTest : public testing::Test {
 public:
  MediaUrlDemuxerTest() : default_url_("http://example.com/") {}

  void InitializeTest(const GURL& gurl) {
    demuxer_.reset(
        new MediaUrlDemuxer(base::ThreadTaskRunnerHandle::Get(), gurl));
  }

  void InitializeTest() { InitializeTest(default_url_); }

  void VerifyCallbackOk(PipelineStatus status) {
    EXPECT_EQ(PIPELINE_OK, status);
  }

  GURL default_url_;
  std::unique_ptr<Demuxer> demuxer_;

  // Necessary, or else base::ThreadTaskRunnerHandle::Get() fails.
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaUrlDemuxerTest);
};

TEST_F(MediaUrlDemuxerTest, BaseCase) {
  InitializeTest();

  EXPECT_EQ(DemuxerStreamProvider::Type::URL, demuxer_->GetType());
  EXPECT_EQ(default_url_, demuxer_->GetUrl());
}

TEST_F(MediaUrlDemuxerTest, AcceptsEmptyStrings) {
  InitializeTest(GURL());

  EXPECT_EQ(GURL::EmptyGURL(), demuxer_->GetUrl());
}

TEST_F(MediaUrlDemuxerTest, InitializeReturnsPipelineOk) {
  InitializeTest();
  demuxer_->Initialize(nullptr,
                       base::Bind(&MediaUrlDemuxerTest::VerifyCallbackOk,
                                  base::Unretained(this)),
                       false);

  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaUrlDemuxerTest, SeekReturnsPipelineOk) {
  InitializeTest();
  demuxer_->Seek(base::TimeDelta(),
                 base::Bind(&MediaUrlDemuxerTest::VerifyCallbackOk,
                            base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
}

}  // namespace media
