// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include "base/bind.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kOne[] = "one";
const char kTwo[] = "two";
const char kThree[] = "three";
const char kFour[] = "four";
#define TEN_LINES "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
const char kLongLog[] = TEN_LINES TEN_LINES TEN_LINES TEN_LINES TEN_LINES;
const char kLogsAttachmentName[] = "system_logs.zip";
}  // namespace

class FeedbackCommonTest : public testing::Test {
 protected:
  FeedbackCommonTest() {
    feedback_ = scoped_refptr<FeedbackCommon>(new FeedbackCommon());
  }

  ~FeedbackCommonTest() override {}

  scoped_refptr<FeedbackCommon> feedback_;
  userfeedback::ExtensionSubmit report_;
};

TEST_F(FeedbackCommonTest, TestBasicData) {
  // Test that basic data can be set and propagates to the request.
  feedback_->set_category_tag(kOne);
  feedback_->set_description(kTwo);
  feedback_->set_page_url(kThree);
  feedback_->set_user_email(kFour);
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(kOne, report_.bucket());
  EXPECT_EQ(kTwo, report_.common_data().description());
  EXPECT_EQ(kThree, report_.web_data().url());
  EXPECT_EQ(kFour, report_.common_data().user_email());
}

TEST_F(FeedbackCommonTest, TestAddLogs) {
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kFour);

  EXPECT_EQ(2U, feedback_->sys_info()->size());
}

TEST_F(FeedbackCommonTest, TestCompressionThreshold) {
  // Add a large and small log, verify that only the small log gets
  // included in the report.
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kLongLog);
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.web_data().product_specific_data_size());
  EXPECT_EQ(kOne, report_.web_data().product_specific_data(0).key());
}

TEST_F(FeedbackCommonTest, TestCompression) {
  // Add a large and small log, verify that an attachment has been
  // added with the right name.
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kLongLog);
  feedback_->CompressLogs();
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.product_specific_binary_data_size());
  EXPECT_EQ(kLogsAttachmentName,
            report_.product_specific_binary_data(0).name());
}
