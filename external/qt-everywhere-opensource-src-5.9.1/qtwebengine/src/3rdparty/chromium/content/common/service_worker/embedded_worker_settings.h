// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_SETTINGS_H_
#define CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_SETTINGS_H_

#include "content/public/common/web_preferences.h"

namespace content {

struct EmbeddedWorkerSettings {
  content::V8CacheOptions v8_cache_options;
  bool data_saver_enabled;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_SETTINGS_H_
