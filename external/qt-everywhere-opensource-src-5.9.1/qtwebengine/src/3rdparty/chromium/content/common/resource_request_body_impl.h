// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_REQUEST_BODY_IMPL_H_
#define CONTENT_COMMON_RESOURCE_REQUEST_BODY_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_request_body.h"
#include "storage/common/data_element.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {

// A struct used to represent upload data. The data field is populated by
// WebURLLoader from the data given as WebHTTPBody.
class CONTENT_EXPORT ResourceRequestBodyImpl : public ResourceRequestBody,
                                               public base::SupportsUserData {
 public:
  typedef storage::DataElement Element;

  ResourceRequestBodyImpl();

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset,
                       uint64_t length,
                       const base::Time& expected_modification_time);
  void AppendBlob(const std::string& uuid);
  void AppendFileSystemFileRange(const GURL& url,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time);

  const std::vector<Element>* elements() const { return &elements_; }
  std::vector<Element>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

  // Returns paths referred to by |elements| of type Element::TYPE_FILE.
  std::vector<base::FilePath> GetReferencedFiles() const;

  // Sets the flag which indicates whether the post data contains sensitive
  // information like passwords.
  void set_contains_sensitive_info(bool contains_sensitive_info) {
    contains_sensitive_info_ = contains_sensitive_info;
  }
  bool contains_sensitive_info() const { return contains_sensitive_info_; }

 private:
  ~ResourceRequestBodyImpl() override;

  std::vector<Element> elements_;
  int64_t identifier_;

  bool contains_sensitive_info_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBodyImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_RESOURCE_REQUEST_BODY_IMPL_H_
