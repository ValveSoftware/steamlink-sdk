// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_

#include <queue>
#include <string>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace feedback {

typedef base::Callback<void(const std::string&)> ReportDataCallback;

class FeedbackReport;

// FeedbackUploader is used to add a feedback report to the queue of reports
// being uploaded. In case uploading a report fails, it is written to disk and
// tried again when it's turn comes up next in the queue.
class FeedbackUploader : public base::SupportsWeakPtr<FeedbackUploader> {
 public:
  FeedbackUploader(const base::FilePath& path,
                   base::SequencedWorkerPool* pool);
  FeedbackUploader(const base::FilePath& path,
                   base::SequencedWorkerPool* pool,
                   const std::string& url);
  virtual ~FeedbackUploader();

  // Queues a report for uploading.
  virtual void QueueReport(const std::string& data);

  base::FilePath GetFeedbackReportsPath() { return report_path_; }

  bool QueueEmpty() const { return reports_queue_.empty(); }

 protected:
  friend class FeedbackUploaderTest;

  struct ReportsUploadTimeComparator {
    bool operator()(const scoped_refptr<FeedbackReport>& a,
                    const scoped_refptr<FeedbackReport>& b) const;
  };

  void Init();

  // Dispatches the report to be uploaded.
  virtual void DispatchReport(const std::string& data) = 0;

  // Update our timer for uploading the next report.
  void UpdateUploadTimer();

  // Requeue this report with a delay.
  void RetryReport(const std::string& data);

  void QueueReportWithDelay(const std::string& data, base::TimeDelta delay);

  void setup_for_test(const ReportDataCallback& dispatch_callback,
                      const base::TimeDelta& retry_delay);

  base::FilePath report_path_;
  // Timer to upload the next report at.
  base::OneShotTimer upload_timer_;
  // Priority queue of reports prioritized by the time the report is supposed
  // to be uploaded at.
  std::priority_queue<scoped_refptr<FeedbackReport>,
                      std::vector<scoped_refptr<FeedbackReport> >,
                      ReportsUploadTimeComparator> reports_queue_;

  ReportDataCallback dispatch_callback_;
  base::TimeDelta retry_delay_;
  std::string url_;
  base::SequencedWorkerPool* pool_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploader);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
