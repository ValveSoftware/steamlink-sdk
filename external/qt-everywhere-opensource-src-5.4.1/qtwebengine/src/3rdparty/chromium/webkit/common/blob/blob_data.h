// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_BLOB_BLOB_DATA_H_
#define WEBKIT_COMMON_BLOB_BLOB_DATA_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/data_element.h"
#include "webkit/common/webkit_storage_common_export.h"

namespace webkit_blob {

class WEBKIT_STORAGE_COMMON_EXPORT BlobData
    : public base::RefCounted<BlobData> {
 public:
  typedef webkit_common::DataElement Item;

  // TODO(michaeln): remove the empty ctor when we fully transition to uuids.
  BlobData();
  explicit BlobData(const std::string& uuid);

  void AppendData(const std::string& data) {
    AppendData(data.c_str(), data.size());
  }

  void AppendData(const char* data, size_t length);

  void AppendFile(const base::FilePath& file_path, uint64 offset, uint64 length,
                  const base::Time& expected_modification_time);
  void AppendBlob(const std::string& uuid, uint64 offset, uint64 length);
  void AppendFileSystemFile(const GURL& url, uint64 offset, uint64 length,
                            const base::Time& expected_modification_time);

  void AttachShareableFileReference(ShareableFileReference* reference) {
    shareable_files_.push_back(reference);
  }

  const std::string& uuid() const { return uuid_; }
  const std::vector<Item>& items() const { return items_; }
  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  const std::string& content_disposition() const {
    return content_disposition_;
  }
  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

  int64 GetMemoryUsage() const;

 private:
  friend class base::RefCounted<BlobData>;
  virtual ~BlobData();

  std::string uuid_;
  std::string content_type_;
  std::string content_disposition_;
  std::vector<Item> items_;
  std::vector<scoped_refptr<ShareableFileReference> > shareable_files_;

  DISALLOW_COPY_AND_ASSIGN(BlobData);
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobData& a, const BlobData& b) {
  if (a.content_type() != b.content_type())
    return false;
  if (a.content_disposition() != b.content_disposition())
    return false;
  if (a.items().size() != b.items().size())
    return false;
  for (size_t i = 0; i < a.items().size(); ++i) {
    if (a.items()[i] != b.items()[i])
      return false;
  }
  return true;
}

inline bool operator!=(const BlobData& a, const BlobData& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace webkit_blob

#endif  // WEBKIT_COMMON_BLOB_BLOB_DATA_H_
