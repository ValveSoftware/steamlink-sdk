// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"

namespace {

const char kMultilineIndicatorString[] = "<multiline>\n";
const char kMultilineStartString[] = "---------- START ----------\n";
const char kMultilineEndString[] = "---------- END ----------\n\n";

const size_t kFeedbackMaxLength = 4 * 1024;
const size_t kFeedbackMaxLineCount = 40;

const base::FilePath::CharType kLogsFilename[] =
    FILE_PATH_LITERAL("system_logs.txt");
const char kLogsAttachmentName[] = "system_logs.zip";

const char kZipExt[] = ".zip";

const char kPngMimeType[] = "image/png";
const char kArbitraryMimeType[] = "application/octet-stream";

// Converts the system logs into a string that we can compress and send
// with the report. This method only converts those logs that we want in
// the compressed zip file sent with the report, hence it ignores any logs
// below the size threshold of what we want compressed.
// TODO(dcheng): This should probably just take advantage of string's move
// constructor.
std::unique_ptr<std::string> LogsToString(
    const FeedbackCommon::SystemLogsMap& sys_info) {
  std::unique_ptr<std::string> syslogs_string(new std::string);
  for (FeedbackCommon::SystemLogsMap::const_iterator it = sys_info.begin();
       it != sys_info.end();
       ++it) {
    std::string key = it->first;
    std::string value = it->second;

    if (FeedbackCommon::BelowCompressionThreshold(value))
      continue;

    base::TrimString(key, "\n ", &key);
    base::TrimString(value, "\n ", &value);

    if (value.find("\n") != std::string::npos) {
      syslogs_string->append(key + "=" + kMultilineIndicatorString +
                             kMultilineStartString + value + "\n" +
                             kMultilineEndString);
    } else {
      syslogs_string->append(key + "=" + value + "\n");
    }
  }
  return syslogs_string;
}

void AddFeedbackData(userfeedback::ExtensionSubmit* feedback_data,
                     const std::string& key,
                     const std::string& value) {
  // Don't bother with empty keys or values.
  if (key.empty() || value.empty())
    return;
  // Create log_value object and add it to the web_data object.
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

// Adds data as an attachment to feedback_data if the data is non-empty.
void AddAttachment(userfeedback::ExtensionSubmit* feedback_data,
                   const char* name,
                   const std::string& data) {
  if (data.empty())
    return;

  userfeedback::ProductSpecificBinaryData* attachment =
      feedback_data->add_product_specific_binary_data();
  attachment->set_mime_type(kArbitraryMimeType);
  attachment->set_name(name);
  attachment->set_data(data);
}

}  // namespace

FeedbackCommon::AttachedFile::AttachedFile(const std::string& filename,
                                           std::unique_ptr<std::string> data)
    : name(filename), data(std::move(data)) {}

FeedbackCommon::AttachedFile::~AttachedFile() {}


FeedbackCommon::FeedbackCommon() : product_id_(0) {}

FeedbackCommon::~FeedbackCommon() {}

// static
bool FeedbackCommon::BelowCompressionThreshold(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = std::count(content.begin(), content.end(), '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}

void FeedbackCommon::CompressFile(const base::FilePath& filename,
                                  const std::string& zipname,
                                  std::unique_ptr<std::string> data) {
  std::unique_ptr<AttachedFile> file(
      new AttachedFile(zipname, base::WrapUnique(new std::string())));
  if (file->name.empty()) {
    // We need to use the UTF8Unsafe methods here to accomodate Windows, which
    // uses wide strings to store filepaths.
    file->name = filename.BaseName().AsUTF8Unsafe();
    file->name.append(kZipExt);
  }
  if (feedback_util::ZipString(filename, *data, file->data.get())) {
    base::AutoLock lock(attachments_lock_);
    attachments_.push_back(file.release());
  }
}

void FeedbackCommon::AddFile(const std::string& filename,
                             std::unique_ptr<std::string> data) {
  base::AutoLock lock(attachments_lock_);
  attachments_.push_back(new AttachedFile(filename, std::move(data)));
}

void FeedbackCommon::AddLog(const std::string& name, const std::string& value) {
  if (!logs_.get())
    logs_ = base::WrapUnique(new SystemLogsMap);
  (*logs_)[name] = value;
}

void FeedbackCommon::AddLogs(std::unique_ptr<SystemLogsMap> logs) {
  if (logs_) {
    logs_->insert(logs->begin(), logs->end());
  } else {
    logs_ = std::move(logs);
  }
}

void FeedbackCommon::CompressLogs() {
  if (!logs_)
    return;
  std::unique_ptr<std::string> logs = LogsToString(*logs_);
  if (!logs->empty()) {
    CompressFile(base::FilePath(kLogsFilename), kLogsAttachmentName,
                 std::move(logs));
  }
}

void FeedbackCommon::AddFilesAndLogsToReport(
    userfeedback::ExtensionSubmit* feedback_data) const {
  if (sys_info()) {
    for (FeedbackCommon::SystemLogsMap::const_iterator i = sys_info()->begin();
         i != sys_info()->end();
         ++i) {
      if (BelowCompressionThreshold(i->second))
        AddFeedbackData(feedback_data, i->first, i->second);
    }
  }

  for (size_t i = 0; i < attachments(); i++) {
    const AttachedFile* file = attachment(i);
    AddAttachment(feedback_data, file->name.c_str(), *file->data.get());
  }
}

void FeedbackCommon::PrepareReport(
    userfeedback::ExtensionSubmit* feedback_data) const {
  // Unused field, needs to be 0 though.
  feedback_data->set_type_id(0);
  feedback_data->set_product_id(product_id_);

  userfeedback::CommonData* common_data = feedback_data->mutable_common_data();
  // We're not using gaia ids, we're using the e-mail field instead.
  common_data->set_gaia_id(0);
  common_data->set_user_email(user_email());
  common_data->set_description(description());
  common_data->set_source_description_language(locale());

  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  web_data->set_url(page_url());
  web_data->mutable_navigator()->set_user_agent(user_agent());

  AddFilesAndLogsToReport(feedback_data);

  if (image() && image()->size()) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);

    // Set that we 'have' dimensions of the screenshot. These dimensions are
    // ignored by the server but are a 'required' field in the protobuf.
    userfeedback::Dimensions dimensions;
    dimensions.set_width(0.0);
    dimensions.set_height(0.0);

    *(screenshot.mutable_dimensions()) = dimensions;
    screenshot.set_binary_content(*image());

    *(feedback_data->mutable_screenshot()) = screenshot;
  }

  if (category_tag().size())
    feedback_data->set_bucket(category_tag());
}
