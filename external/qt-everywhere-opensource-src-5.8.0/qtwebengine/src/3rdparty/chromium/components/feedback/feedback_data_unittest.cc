// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include <memory>
#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kHistograms[] = "";
const char kImageData[] = "";
const char kFileData[] = "";

using content::BrowserThread;

class MockUploader : public feedback::FeedbackUploader, public KeyedService {
 public:
  MockUploader(content::BrowserContext* context)
      : FeedbackUploader(context ? context->GetPath() : base::FilePath(),
                         BrowserThread::GetBlockingPool()) {}

  MOCK_METHOD1(DispatchReport, void(const std::string&));
};

std::unique_ptr<KeyedService> CreateFeedbackUploaderService(
    content::BrowserContext* context) {
  std::unique_ptr<MockUploader> uploader(new MockUploader(context));
  EXPECT_CALL(*uploader, DispatchReport(testing::_)).Times(1);
  return std::move(uploader);
}

std::unique_ptr<std::string> MakeScoped(const char* str) {
  return base::WrapUnique(new std::string(str));
}

}  // namespace

namespace feedback {

class FeedbackDataTest : public testing::Test {
 protected:
  FeedbackDataTest()
      : context_(new content::TestBrowserContext()),
        prefs_(new TestingPrefServiceSimple()),
        data_(new FeedbackData()),
        ui_thread_(content::BrowserThread::UI, &message_loop_) {
    user_prefs::UserPrefs::Set(context_.get(), prefs_.get());
    data_->set_context(context_.get());
    data_->set_send_report_callback(base::Bind(
        &FeedbackDataTest::set_send_report_callback, base::Unretained(this)));

    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        context_.get(), &CreateFeedbackUploaderService);
  }

  void Send() {
    bool attached_file_completed =
        data_->attached_file_uuid().empty();
    bool screenshot_completed =
        data_->screenshot_uuid().empty();

    if (screenshot_completed && attached_file_completed) {
      data_->OnFeedbackPageDataComplete();
    }
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop());
    quit_closure_ = run_loop_->QuitClosure();
    run_loop_->Run();
  }

  void set_send_report_callback(scoped_refptr<FeedbackData> data) {
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::TestBrowserContext> context_;
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<FeedbackData> data_;
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(FeedbackDataTest, ReportSending) {
  data_->SetAndCompressHistograms(MakeScoped(kHistograms));
  data_->set_image(MakeScoped(kImageData));
  data_->AttachAndCompressFileData(MakeScoped(kFileData));
  Send();
  RunMessageLoop();
  EXPECT_TRUE(data_->IsDataComplete());
}

}  // namespace feedback
