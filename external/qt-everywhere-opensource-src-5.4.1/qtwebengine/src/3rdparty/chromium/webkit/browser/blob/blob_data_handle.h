// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_BLOB_BLOB_DATA_HANDLE_H_
#define WEBKIT_BROWSER_BLOB_BLOB_DATA_HANDLE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace webkit_blob {

class BlobData;
class BlobStorageContext;

// A scoper object for use in chrome's main browser process, ensures
// the underlying BlobData and its uuid remain in BlobStorageContext's
// collection for the duration. This object has delete semantics and
// maybe deleted on any thread.
class WEBKIT_STORAGE_BROWSER_EXPORT BlobDataHandle
    : public base::SupportsUserData::Data {
 public:
  BlobDataHandle(const BlobDataHandle& other);  // May be copied on any thread.
  virtual ~BlobDataHandle();  // Maybe be deleted on any thread.
  BlobData* data() const;  // May only be accessed on the IO thread.

  std::string uuid() const;  // May be accessed on any thread.

 private:
  class BlobDataHandleShared
      : public base::RefCountedThreadSafe<BlobDataHandleShared> {
   public:
    BlobDataHandleShared(BlobData* blob_data,
                         BlobStorageContext* context,
                         base::SequencedTaskRunner* task_runner);

    BlobData* data() const;
    const std::string& uuid() const;

   private:
    friend class base::DeleteHelper<BlobDataHandleShared>;
    friend class base::RefCountedThreadSafe<BlobDataHandleShared>;
    friend class BlobDataHandle;

    virtual ~BlobDataHandleShared();

    scoped_refptr<BlobData> blob_data_;
    base::WeakPtr<BlobStorageContext> context_;

    DISALLOW_COPY_AND_ASSIGN(BlobDataHandleShared);
  };

  friend class BlobStorageContext;
  BlobDataHandle(BlobData* blob_data, BlobStorageContext* context,
                 base::SequencedTaskRunner* task_runner);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<BlobDataHandleShared> shared_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_BROWSER_BLOB_BLOB_DATA_HANDLE_H_
