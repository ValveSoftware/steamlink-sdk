// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/browser/feedback/system_logs/scrubbed_system_logs_fetcher.h"
#include "chrome/common/extensions/api/feedback_private.h"
#include "components/feedback/feedback_data.h"

class Profile;

namespace extensions {

using SystemInformationList =
    std::vector<api::feedback_private::SystemInformation>;

// The feedback service provides the ability to gather the various pieces of
// data needed to send a feedback report and then send the report once all
// the pieces are available.
class FeedbackService : public base::SupportsWeakPtr<FeedbackService> {
 public:
  using SendFeedbackCallback = base::Callback<void(bool)>;
  using GetSystemInformationCallback =
      base::Callback<void(const SystemInformationList&)>;

  FeedbackService();
  virtual ~FeedbackService();

  // Sends a feedback report.
  void SendFeedback(Profile* profile,
                    scoped_refptr<feedback::FeedbackData> feedback_data,
                    const SendFeedbackCallback& callback);

  // Start to gather system information.
  // The |callback| will be invoked once the query is completed.
  void GetSystemInformation(const GetSystemInformationCallback& callback);

 private:
  // Callbacks to receive blob data.
  void AttachedFileCallback(scoped_refptr<feedback::FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback,
                            std::unique_ptr<std::string> data,
                            int64_t total_blob_length);
  void ScreenshotCallback(scoped_refptr<feedback::FeedbackData> feedback_data,
                          const SendFeedbackCallback& callback,
                          std::unique_ptr<std::string> data,
                          int64_t total_blob_length);

  void OnSystemLogsFetchComplete(
      const GetSystemInformationCallback& callback,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);

  // Checks if we have read all the blobs we need to; signals the feedback
  // data object once all the requisite data has been populated.
  void CompleteSendFeedback(scoped_refptr<feedback::FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(FeedbackService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
