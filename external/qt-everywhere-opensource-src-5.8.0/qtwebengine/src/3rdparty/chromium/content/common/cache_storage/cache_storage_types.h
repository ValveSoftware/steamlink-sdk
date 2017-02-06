// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_
#define CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_

#include <map>
#include <string>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Controls how requests are matched in the Cache API.
struct CONTENT_EXPORT CacheStorageCacheQueryParams {
  CacheStorageCacheQueryParams();

  bool ignore_search;
  bool ignore_method;
  bool ignore_vary;
  base::string16 cache_name;
};

// The type of a single batch operation in the Cache API.
enum CacheStorageCacheOperationType {
  CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_LAST =
      CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE
};

// A single batch operation for the Cache API.
struct CONTENT_EXPORT CacheStorageBatchOperation {
  CacheStorageBatchOperation();
  CacheStorageBatchOperation(const CacheStorageBatchOperation& other);

  CacheStorageCacheOperationType operation_type;
  ServiceWorkerFetchRequest request;
  ServiceWorkerResponse response;
  CacheStorageCacheQueryParams match_params;
};

// This enum is used in histograms, so do not change the ordering and always
// append new types to the end.
enum CacheStorageError {
  CACHE_STORAGE_OK = 0,
  CACHE_STORAGE_ERROR_EXISTS,
  CACHE_STORAGE_ERROR_STORAGE,
  CACHE_STORAGE_ERROR_NOT_FOUND,
  CACHE_STORAGE_ERROR_QUOTA_EXCEEDED,
  CACHE_STORAGE_ERROR_CACHE_NAME_NOT_FOUND,
  CACHE_STORAGE_ERROR_LAST = CACHE_STORAGE_ERROR_CACHE_NAME_NOT_FOUND
};

}  // namespace content

#endif  // CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_
