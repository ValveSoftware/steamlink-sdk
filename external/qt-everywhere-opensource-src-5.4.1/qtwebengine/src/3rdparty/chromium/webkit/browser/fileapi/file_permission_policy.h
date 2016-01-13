// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_PERMISSION_POLICY_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_PERMISSION_POLICY_H_

#include "webkit/browser/webkit_storage_browser_export.h"

namespace fileapi {

enum FilePermissionPolicy {
  // Any access should be always denied.
  FILE_PERMISSION_ALWAYS_DENY = 0x0,

  // Access is sandboxed, no extra permission check is necessary.
  FILE_PERMISSION_SANDBOX = 1 << 0,

  // Access should be restricted to read-only.
  FILE_PERMISSION_READ_ONLY = 1 << 1,

  // Access should be examined by per-file permission policy.
  FILE_PERMISSION_USE_FILE_PERMISSION = 1 << 2,
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_PERMISSION_POLICY_H_
