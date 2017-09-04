// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_export.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "url/gurl.h"

#ifndef CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_H_
#define CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_H_

namespace content {

struct CONTENT_EXPORT EmbeddedWorkerStartParams {
  EmbeddedWorkerStartParams();

  int embedded_worker_id;
  int64_t service_worker_version_id;
  GURL scope;
  GURL script_url;
  int worker_devtools_agent_route_id;
  bool pause_after_download;
  bool wait_for_debugger;
  bool is_installed;
  EmbeddedWorkerSettings settings;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_H_
