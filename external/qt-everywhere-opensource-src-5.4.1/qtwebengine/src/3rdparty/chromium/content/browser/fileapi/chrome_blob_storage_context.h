// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_CHROME_BLOB_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_FILEAPI_CHROME_BLOB_STORAGE_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"

namespace webkit_blob {
class BlobStorageContext;
}

namespace content {
class BlobHandle;
class BrowserContext;
struct ChromeBlobStorageContextDeleter;

// A context class that keeps track of BlobStorageController used by the chrome.
// There is an instance associated with each BrowserContext. There could be
// multiple URLRequestContexts in the same browser context that refers to the
// same instance.
//
// All methods, except the ctor, are expected to be called on
// the IO thread (unless specifically called out in doc comments).
class CONTENT_EXPORT ChromeBlobStorageContext
    : public base::RefCountedThreadSafe<ChromeBlobStorageContext,
                                        ChromeBlobStorageContextDeleter> {
 public:
  ChromeBlobStorageContext();

  static ChromeBlobStorageContext* GetFor(
      BrowserContext* browser_context);

  void InitializeOnIOThread();

  webkit_blob::BlobStorageContext* context() const {
    return context_.get();
  }

  // Returns a NULL scoped_ptr on failure.
  scoped_ptr<BlobHandle> CreateMemoryBackedBlob(const char* data,
                                                size_t length);

 protected:
  virtual ~ChromeBlobStorageContext();

 private:
  friend class base::DeleteHelper<ChromeBlobStorageContext>;
  friend class base::RefCountedThreadSafe<ChromeBlobStorageContext,
                                          ChromeBlobStorageContextDeleter>;
  friend struct ChromeBlobStorageContextDeleter;

  void DeleteOnCorrectThread() const;

  scoped_ptr<webkit_blob::BlobStorageContext> context_;
};

struct ChromeBlobStorageContextDeleter {
  static void Destruct(const ChromeBlobStorageContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_CHROME_BLOB_STORAGE_CONTEXT_H_
