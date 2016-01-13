// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/devtools_auth.h"

#include <unistd.h>
#include <sys/types.h>

#include "base/logging.h"

namespace content {

bool CanUserConnectToDevTools(uid_t uid, gid_t gid) {
  struct passwd* creds = getpwuid(uid);
  if (!creds || !creds->pw_name) {
    LOG(WARNING) << "DevTools: can't obtain creds for uid " << uid;
    return false;
  }
  if (gid == uid &&
      (strcmp("root", creds->pw_name) == 0 ||   // For rooted devices
       strcmp("shell", creds->pw_name) == 0 ||  // For non-rooted devices
       uid == getuid())) {  // From processes signed with the same key
    return true;
  }
  LOG(WARNING) << "DevTools: connection attempt from " << creds->pw_name;
  return false;
}

}  // namespace content
