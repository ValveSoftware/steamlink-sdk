// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_REQUEST_BODY_H_
#define CONTENT_COMMON_RESOURCE_REQUEST_BODY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "webkit/common/data_element.h"

namespace base {
class FilePath;
}

namespace content {

// A struct used to represent upload data. The data field is populated by
// WebURLLoader from the data given as WebHTTPBody.
class CONTENT_EXPORT ResourceRequestBody
    : public base::RefCounted<ResourceRequestBody>,
      public base::SupportsUserData {
 public:
  typedef webkit_common::DataElement Element;

  ResourceRequestBody();

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64 offset, uint64 length,
                       const base::Time& expected_modification_time);
  void AppendBlob(const std::string& uuid);
  void AppendFileSystemFileRange(const GURL& url, uint64 offset, uint64 length,
                                 const base::Time& expected_modification_time);

  const std::vector<Element>* elements() const { return &elements_; }
  std::vector<Element>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<ResourceRequestBody>;
  virtual ~ResourceRequestBody();

  std::vector<Element> elements_;
  int64 identifier_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace content

#endif  // CONTENT_COMMON_RESOURCE_REQUEST_BODY_H_
