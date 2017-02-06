// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"

namespace userfeedback {
class ExtensionSubmit;
}

namespace feedback_util {
bool ZipString(const base::FilePath& filename,
               const std::string& data,
               std::string* compressed_data);
}

// This is the base class for FeedbackData. It primarily knows about
// data common to all feedback reports and how to zip things.
class FeedbackCommon : public base::RefCountedThreadSafe<FeedbackCommon> {
 public:
  typedef std::map<std::string, std::string> SystemLogsMap;

  struct AttachedFile {
    explicit AttachedFile(const std::string& filename,
                          std::unique_ptr<std::string> data);
    ~AttachedFile();

    std::string name;
    std::unique_ptr<std::string> data;
  };

  // Determine if the given feedback value is small enough to not need to
  // be compressed.
  static bool BelowCompressionThreshold(const std::string& content);

  FeedbackCommon();

  void CompressFile(const base::FilePath& filename,
                    const std::string& zipname,
                    std::unique_ptr<std::string> data);
  void AddFile(const std::string& filename, std::unique_ptr<std::string> data);

  void AddLog(const std::string& name, const std::string& value);
  void AddLogs(std::unique_ptr<SystemLogsMap> logs);
  void CompressLogs();

  void AddFilesAndLogsToReport(
      userfeedback::ExtensionSubmit* feedback_data) const;

  // Fill in |feedback_data| with all the data that we have collected.
  // CompressLogs() must have already been called.
  void PrepareReport(userfeedback::ExtensionSubmit* feedback_data) const;

  // Getters
  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  const std::string* image() const { return image_.get(); }
  const SystemLogsMap* sys_info() const { return logs_.get(); }
  int32_t product_id() const { return product_id_; }
  std::string user_agent() const { return user_agent_; }
  std::string locale() const { return locale_; }

  const AttachedFile* attachment(size_t i) const { return attachments_[i]; }
  size_t attachments() const { return attachments_.size(); }

  // Setters
  void set_category_tag(const std::string& category_tag) {
    category_tag_ = category_tag;
  }
  void set_page_url(const std::string& page_url) { page_url_ = page_url; }
  void set_description(const std::string& description) {
    description_ = description;
  }
  void set_user_email(const std::string& user_email) {
    user_email_ = user_email;
  }
  void set_image(std::unique_ptr<std::string> image) {
    image_ = std::move(image);
  }
  void set_product_id(int32_t product_id) { product_id_ = product_id; }
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }
  void set_locale(const std::string& locale) { locale_ = locale; }

 protected:
  friend class base::RefCountedThreadSafe<FeedbackCommon>;
  friend class FeedbackCommonTest;

  virtual ~FeedbackCommon();

 private:
  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  int32_t product_id_;
  std::string user_agent_;
  std::string locale_;

  std::unique_ptr<std::string> image_;

  // It is possible that multiple attachment add calls are running in
  // parallel, so synchronize access.
  base::Lock attachments_lock_;
  ScopedVector<AttachedFile> attachments_;

  std::unique_ptr<SystemLogsMap> logs_;
};

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_
