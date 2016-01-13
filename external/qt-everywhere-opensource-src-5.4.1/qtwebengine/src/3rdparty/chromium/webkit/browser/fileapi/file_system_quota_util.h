// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/files/file.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemContext;
class QuotaReservation;

// An abstract interface that provides common quota-related utility functions
// for file_system_quota_client.
// All the methods of this class are synchronous and need to be called on
// the thread that the method name implies.
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemQuotaUtil {
 public:
  virtual ~FileSystemQuotaUtil() {}

  // Deletes the data on the origin and reports the amount of deleted data
  // to the quota manager via |proxy|.
  virtual base::File::Error DeleteOriginDataOnFileTaskRunner(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) = 0;

  virtual void GetOriginsForTypeOnFileTaskRunner(
      fileapi::FileSystemType type,
      std::set<GURL>* origins) = 0;

  virtual void GetOriginsForHostOnFileTaskRunner(
      fileapi::FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) = 0;

  // Returns the amount of data used for the origin for usage tracking.
  virtual int64 GetOriginUsageOnFileTaskRunner(
      fileapi::FileSystemContext* file_system_context,
      const GURL& origin_url,
      fileapi::FileSystemType type) = 0;

  // Creates new reservation object for the origin and the type.
  virtual scoped_refptr<QuotaReservation>
      CreateQuotaReservationOnFileTaskRunner(
          const GURL& origin_url,
          FileSystemType type) = 0;

  virtual void AddFileUpdateObserver(
      FileSystemType type,
      FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) = 0;
  virtual void AddFileChangeObserver(
      FileSystemType type,
      FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) = 0;
  virtual void AddFileAccessObserver(
      FileSystemType type,
      FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) = 0;

  // Returns the observer list for |type|, or returns NULL if any observers
  // have not been registered on |type|.
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const = 0;
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const = 0;
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
