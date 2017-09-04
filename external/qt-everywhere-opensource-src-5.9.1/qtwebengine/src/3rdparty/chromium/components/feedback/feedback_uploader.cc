// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/feedback/feedback_report.h"

namespace feedback {
namespace {

const char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

const int64_t kRetryDelayMinutes = 60;

const base::FilePath::CharType kFeedbackReportPath[] =
    FILE_PATH_LITERAL("Feedback Reports");

}  // namespace

bool FeedbackUploader::ReportsUploadTimeComparator::operator()(
    const scoped_refptr<FeedbackReport>& a,
    const scoped_refptr<FeedbackReport>& b) const {
  return a->upload_at() > b->upload_at();
}

FeedbackUploader::FeedbackUploader(const base::FilePath& path,
                                   base::SequencedWorkerPool* pool)
    : report_path_(path.Append(kFeedbackReportPath)),
      retry_delay_(base::TimeDelta::FromMinutes(kRetryDelayMinutes)),
      url_(kFeedbackPostUrl),
      pool_(pool) {
  Init();
}

FeedbackUploader::FeedbackUploader(const base::FilePath& path,
                                   base::SequencedWorkerPool* pool,
                                   const std::string& url)
    : report_path_(path.Append(kFeedbackReportPath)),
      retry_delay_(base::TimeDelta::FromMinutes(kRetryDelayMinutes)),
      url_(url),
      pool_(pool) {
  Init();
}

FeedbackUploader::~FeedbackUploader() {}

void FeedbackUploader::Init() {
  dispatch_callback_ = base::Bind(&FeedbackUploader::DispatchReport,
                                  AsWeakPtr());
}

void FeedbackUploader::QueueReport(const std::string& data) {
  QueueReportWithDelay(data, base::TimeDelta());
}

void FeedbackUploader::UpdateUploadTimer() {
  if (reports_queue_.empty())
    return;

  scoped_refptr<FeedbackReport> report = reports_queue_.top();
  base::Time now = base::Time::Now();
  if (report->upload_at() <= now) {
    reports_queue_.pop();
    dispatch_callback_.Run(report->data());
    report->DeleteReportOnDisk();
  } else {
    // Stop the old timer and start an updated one.
    if (upload_timer_.IsRunning())
      upload_timer_.Stop();
    upload_timer_.Start(
        FROM_HERE, report->upload_at() - now, this,
        &FeedbackUploader::UpdateUploadTimer);
  }
}

void FeedbackUploader::RetryReport(const std::string& data) {
  QueueReportWithDelay(data, retry_delay_);
}

void FeedbackUploader::QueueReportWithDelay(const std::string& data,
                                            base::TimeDelta delay) {
  // Uses a BLOCK_SHUTDOWN file task runner because we really don't want to
  // lose reports.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      pool_->GetSequencedTaskRunnerWithShutdownBehavior(
          pool_->GetSequenceToken(),
          base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  reports_queue_.push(new FeedbackReport(report_path_,
                                         base::Time::Now() + delay,
                                         data,
                                         task_runner));
  UpdateUploadTimer();
}

void FeedbackUploader::setup_for_test(
    const ReportDataCallback& dispatch_callback,
    const base::TimeDelta& retry_delay) {
  dispatch_callback_ = dispatch_callback;
  retry_delay_ = retry_delay;
}

}  // namespace feedback
