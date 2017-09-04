// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_ui_service.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/previews/core/previews_io_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      PreviewsIOData* previews_io_data,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store)
      : PreviewsUIService(previews_io_data,
                          io_task_runner,
                          std::move(previews_opt_out_store)),
        io_data_set_(false) {}
  ~TestPreviewsUIService() override {}

  // Set |io_data_set_| to true and use base class functionality.
  void SetIOData(base::WeakPtr<PreviewsIOData> previews_io_data) override {
    io_data_set_ = true;
    PreviewsUIService::SetIOData(previews_io_data);
  }

  // Whether SetIOData was called.
  bool io_data_set() { return io_data_set_; }

 private:
  // Whether SetIOData was called.
  bool io_data_set_;
};

class PreviewsUIServiceTest : public testing::Test {
 public:
  PreviewsUIServiceTest() {}

  ~PreviewsUIServiceTest() override {}

  void set_io_data(std::unique_ptr<PreviewsIOData> io_data) {
    io_data_ = std::move(io_data);
  }

  PreviewsIOData* io_data() { return io_data_.get(); }

  void set_ui_service(std::unique_ptr<TestPreviewsUIService> ui_service) {
    ui_service_ = std::move(ui_service);
  }

  TestPreviewsUIService* ui_service() { return ui_service_.get(); }

 protected:
  // Run this test on a single thread.
  base::MessageLoopForIO loop_;

 private:
  std::unique_ptr<PreviewsIOData> io_data_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
};

}  // namespace

TEST_F(PreviewsUIServiceTest, TestInitialization) {
  set_io_data(base::WrapUnique(
      new PreviewsIOData(loop_.task_runner(), loop_.task_runner())));
  set_ui_service(base::WrapUnique(
      new TestPreviewsUIService(io_data(), loop_.task_runner(), nullptr)));
  base::RunLoop().RunUntilIdle();
  // After the outstanding posted tasks have run, SetIOData should have been
  // called for |ui_service_|.
  EXPECT_TRUE(ui_service()->io_data_set());
}

}  // namespace previews
