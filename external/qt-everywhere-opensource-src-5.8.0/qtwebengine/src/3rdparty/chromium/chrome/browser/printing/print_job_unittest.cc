// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

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

namespace {

class TestSource : public printing::PrintedPagesSource {
 public:
  base::string16 RenderSourceName() override { return base::string16(); }
};

class TestPrintJobWorker : public printing::PrintJobWorker {
 public:
  explicit TestPrintJobWorker(printing::PrintJobWorkerOwner* owner)
      : printing::PrintJobWorker(content::ChildProcessHost::kInvalidUniqueID,
                                 content::ChildProcessHost::kInvalidUniqueID,
                                 owner) {}
  friend class TestOwner;
};

class TestOwner : public printing::PrintJobWorkerOwner {
 public:
  void GetSettingsDone(const printing::PrintSettings& new_settings,
                       printing::PrintingContext::Result result) override {
    EXPECT_FALSE(true);
  }
  printing::PrintJobWorker* DetachWorker(
      printing::PrintJobWorkerOwner* new_owner) override {
    // We're screwing up here since we're calling worker from the main thread.
    // That's fine for testing. It is actually simulating PrinterQuery behavior.
    TestPrintJobWorker* worker(new TestPrintJobWorker(new_owner));
    EXPECT_TRUE(worker->Start());
    worker->printing_context()->UseDefaultSettings();
    settings_ = worker->printing_context()->settings();
    return worker;
  }
  const printing::PrintSettings& settings() const override { return settings_; }
  int cookie() const override { return 42; }

 private:
  ~TestOwner() override {}

  printing::PrintSettings settings_;
};

class TestPrintJob : public printing::PrintJob {
 public:
  explicit TestPrintJob(volatile bool* check) : check_(check) {
  }
 private:
  ~TestPrintJob() override { *check_ = true; }
  volatile bool* check_;
};

class TestPrintNotifObserv : public content::NotificationObserver {
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
  TestPrintNotifObserv observ;
  registrar_.Add(&observ,
                 content::NOTIFICATION_ALL,
                 content::NotificationService::AllSources());
  volatile bool check = false;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&check));
  EXPECT_TRUE(job->RunsTasksOnCurrentThread());
  scoped_refptr<TestOwner> owner(new TestOwner);
  TestSource source;
  job->Initialize(owner.get(), &source, 1);
  job->Stop();
  while (job->document()) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(job->document());
  job = NULL;
  while (!check) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(check);
}

TEST(PrintJobTest, SimplePrintLateInit) {
  volatile bool check = false;
  base::MessageLoop current;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&check));
  job = NULL;
  EXPECT_TRUE(check);
  /* TODO(maruel): Test these.
  job->Initialize()
  job->Observe();
  job->GetSettingsDone();
  job->DetachWorker();
  job->message_loop();
  job->settings();
  job->cookie();
  job->GetSettings(printing::DEFAULTS, printing::ASK_USER, NULL);
  job->StartPrinting();
  job->Stop();
  job->Cancel();
  job->RequestMissingPages();
  job->FlushJob(timeout);
  job->DisconnectSource();
  job->is_job_pending();
  job->document();
  // Private
  job->UpdatePrintedDocument(NULL);
  scoped_refptr<printing::JobEventDetails> event_details;
  job->OnNotifyPrintJobEvent(event_details);
  job->OnDocumentDone();
  job->ControlledWorkerShutdown();
  */
}
