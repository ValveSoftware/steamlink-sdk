// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "printing/printed_pages_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

class TestSource : public PrintedPagesSource {
 public:
  base::string16 RenderSourceName() override { return base::string16(); }
};

class TestPrintJobWorker : public PrintJobWorker {
 public:
  explicit TestPrintJobWorker(PrintJobWorkerOwner* owner)
      : PrintJobWorker(content::ChildProcessHost::kInvalidUniqueID,
                       content::ChildProcessHost::kInvalidUniqueID,
                       owner) {}
  friend class TestOwner;
};

class TestOwner : public PrintJobWorkerOwner {
 public:
  TestOwner() {}

  void GetSettingsDone(const PrintSettings& new_settings,
                       PrintingContext::Result result) override {
    EXPECT_FALSE(true);
  }

  std::unique_ptr<PrintJobWorker> DetachWorker(
      PrintJobWorkerOwner* new_owner) override {
    // We're screwing up here since we're calling worker from the main thread.
    // That's fine for testing. It is actually simulating PrinterQuery behavior.
    auto worker = base::MakeUnique<TestPrintJobWorker>(new_owner);
    EXPECT_TRUE(worker->Start());
    worker->printing_context()->UseDefaultSettings();
    settings_ = worker->printing_context()->settings();
    return std::move(worker);
  }

  const PrintSettings& settings() const override { return settings_; }

  int cookie() const override { return 42; }

 private:
  ~TestOwner() override {}

  PrintSettings settings_;

  DISALLOW_COPY_AND_ASSIGN(TestOwner);
};

class TestPrintJob : public PrintJob {
 public:
  explicit TestPrintJob(volatile bool* check) : check_(check) {
  }
 private:
  ~TestPrintJob() override { *check_ = true; }
  volatile bool* check_;
};

class TestPrintNotificationObserver : public content::NotificationObserver {
 public:
  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ADD_FAILURE();
  }
};

}  // namespace

TEST(PrintJobTest, SimplePrint) {
  // Test the multi-threaded nature of PrintJob to make sure we can use it with
  // known lifetime.

  content::TestBrowserThreadBundle thread_bundle_;
  content::NotificationRegistrar registrar_;
  TestPrintNotificationObserver observer;
  registrar_.Add(&observer,
                 content::NOTIFICATION_ALL,
                 content::NotificationService::AllSources());
  volatile bool check = false;
  scoped_refptr<PrintJob> job(new TestPrintJob(&check));
  EXPECT_TRUE(job->RunsTasksOnCurrentThread());
  scoped_refptr<TestOwner> owner(new TestOwner);
  TestSource source;
  job->Initialize(owner.get(), &source, 1);
  job->Stop();
  while (job->document()) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(job->document());
  job = nullptr;
  while (!check) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(check);
}

TEST(PrintJobTest, SimplePrintLateInit) {
  volatile bool check = false;
  base::MessageLoop current;
  scoped_refptr<PrintJob> job(new TestPrintJob(&check));
  job = nullptr;
  EXPECT_TRUE(check);
  /* TODO(maruel): Test these.
  job->Initialize()
  job->Observe();
  job->GetSettingsDone();
  job->DetachWorker();
  job->message_loop();
  job->settings();
  job->cookie();
  job->GetSettings(DEFAULTS, ASK_USER, nullptr);
  job->StartPrinting();
  job->Stop();
  job->Cancel();
  job->RequestMissingPages();
  job->FlushJob(timeout);
  job->DisconnectSource();
  job->is_job_pending();
  job->document();
  // Private
  job->UpdatePrintedDocument(nullptr);
  scoped_refptr<JobEventDetails> event_details;
  job->OnNotifyPrintJobEvent(event_details);
  job->OnDocumentDone();
  job->ControlledWorkerShutdown();
  */
}

}  // namespace printing
