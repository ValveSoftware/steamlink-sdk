// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using extensions::api::feedback_private::SystemInformation;
using feedback::FeedbackData;

namespace extensions {

namespace {

void PopulateSystemInfo(SystemInformationList* sys_info_list,
                        const std::string& key,
                        const std::string& value) {
  base::DictionaryValue sys_info_value;
  sys_info_value.Set("key", new base::StringValue(key));
  sys_info_value.Set("value", new base::StringValue(value));

  SystemInformation sys_info;
  SystemInformation::Populate(sys_info_value, &sys_info);

  sys_info_list->push_back(std::move(sys_info));
}

}  // namespace

FeedbackService::FeedbackService() {
}

FeedbackService::~FeedbackService() {
}

void FeedbackService::SendFeedback(
    Profile* profile,
    scoped_refptr<FeedbackData> feedback_data,
    const SendFeedbackCallback& callback) {
  feedback_data->set_locale(g_browser_process->GetApplicationLocale());
  feedback_data->set_user_agent(GetUserAgent());

  if (!feedback_data->attached_file_uuid().empty()) {
    // Self-deleting object.
    BlobReader* attached_file_reader =
        new BlobReader(profile, feedback_data->attached_file_uuid(),
                       base::Bind(&FeedbackService::AttachedFileCallback,
                                  AsWeakPtr(), feedback_data, callback));
    attached_file_reader->Start();
  }

  if (!feedback_data->screenshot_uuid().empty()) {
    // Self-deleting object.
    BlobReader* screenshot_reader =
        new BlobReader(profile, feedback_data->screenshot_uuid(),
                       base::Bind(&FeedbackService::ScreenshotCallback,
                                  AsWeakPtr(), feedback_data, callback));
    screenshot_reader->Start();
  }

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::GetSystemInformation(
    const GetSystemInformationCallback& callback) {
  system_logs::ScrubbedSystemLogsFetcher* fetcher =
      new system_logs::ScrubbedSystemLogsFetcher();
  fetcher->Fetch(base::Bind(&FeedbackService::OnSystemLogsFetchComplete,
                            AsWeakPtr(), callback));
}

void FeedbackService::AttachedFileCallback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback,
    std::unique_ptr<std::string> data,
    int64_t /* total_blob_length */) {
  feedback_data->set_attached_file_uuid(std::string());
  if (data)
    feedback_data->AttachAndCompressFileData(std::move(data));

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::ScreenshotCallback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback,
    std::unique_ptr<std::string> data,
    int64_t /* total_blob_length */) {
  feedback_data->set_screenshot_uuid(std::string());
  if (data)
    feedback_data->set_image(std::move(data));

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::OnSystemLogsFetchComplete(
    const GetSystemInformationCallback& callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info_map) {
  SystemInformationList sys_info_list;
  if (sys_info_map.get()) {
    for (const auto& itr : *sys_info_map)
      PopulateSystemInfo(&sys_info_list, itr.first, itr.second);
  }

  callback.Run(sys_info_list);
}

void FeedbackService::CompleteSendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback) {
  // A particular data collection is considered completed if,
  // a.) The blob URL is invalid - this will either happen because we never had
  //     a URL and never needed to read this data, or that the data read failed
  //     and we set it to invalid in the data read callback.
  // b.) The associated data object exists, meaning that the data has been read
  //     and the read callback has updated the associated data on the feedback
  //     object.
  const bool attached_file_completed =
      feedback_data->attached_file_uuid().empty();
  const bool screenshot_completed = feedback_data->screenshot_uuid().empty();

  if (screenshot_completed && attached_file_completed) {
    // Signal the feedback object that the data from the feedback page has been
    // filled - the object will manage sending of the actual report.
    feedback_data->OnFeedbackPageDataComplete();

    // TODO(rkc): Change this once we have FeedbackData/Util refactored to
    // report the status of the report being sent.
    callback.Run(true);
  }
}

}  // namespace extensions
