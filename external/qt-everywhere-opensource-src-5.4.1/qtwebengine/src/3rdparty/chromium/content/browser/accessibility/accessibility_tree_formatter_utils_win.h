// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT base::string16 IAccessibleRoleToString(int32 ia_role);
CONTENT_EXPORT base::string16 IAccessible2RoleToString(int32 ia_role);
CONTENT_EXPORT base::string16 IAccessibleStateToString(int32 ia_state);
CONTENT_EXPORT void IAccessibleStateToStringVector(
    int32 ia_state, std::vector<base::string16>* result);
CONTENT_EXPORT base::string16 IAccessible2StateToString(int32 ia2_state);
CONTENT_EXPORT void IAccessible2StateToStringVector(
  int32 ia_state, std::vector<base::string16>* result);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_
