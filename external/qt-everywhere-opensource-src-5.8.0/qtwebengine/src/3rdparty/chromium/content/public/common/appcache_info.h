// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_APPCACHE_INFO_H_
#define CONTENT_PUBLIC_COMMON_APPCACHE_INFO_H_

#include <stdint.h>

#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

static const int kAppCacheNoHostId = 0;
static const int64_t kAppCacheNoCacheId = 0;
static const int64_t kAppCacheNoResponseId = 0;
static const int64_t kAppCacheUnknownCacheId = -1;

enum AppCacheStatus {
  APPCACHE_STATUS_UNCACHED,
  APPCACHE_STATUS_IDLE,
  APPCACHE_STATUS_CHECKING,
  APPCACHE_STATUS_DOWNLOADING,
  APPCACHE_STATUS_UPDATE_READY,
  APPCACHE_STATUS_OBSOLETE,
  APPCACHE_STATUS_LAST = APPCACHE_STATUS_OBSOLETE
};

struct CONTENT_EXPORT AppCacheInfo {
  AppCacheInfo();
  AppCacheInfo(const AppCacheInfo& other);
  ~AppCacheInfo();

  GURL manifest_url;
  base::Time creation_time;
  base::Time last_update_time;
  base::Time last_access_time;
  int64_t cache_id;
  int64_t group_id;
  AppCacheStatus status;
  int64_t size;
  bool is_complete;
};

typedef std::vector<AppCacheInfo> AppCacheInfoVector;

}  // namespace

#endif  // CONTENT_PUBLIC_COMMON_APPCACHE_INFO_H_
