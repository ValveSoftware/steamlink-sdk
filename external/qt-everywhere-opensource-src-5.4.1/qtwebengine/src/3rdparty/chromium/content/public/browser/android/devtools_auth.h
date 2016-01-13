// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AUTH_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AUTH_ANDROID_H_

#include "content/common/content_export.h"

#include <pwd.h>

namespace content {

// Returns true if the given user/group pair is authorized to connect to the
// devtools server, false if not.
CONTENT_EXPORT bool CanUserConnectToDevTools(uid_t uid, gid_t gid);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AUTH_ANDROID_H_
