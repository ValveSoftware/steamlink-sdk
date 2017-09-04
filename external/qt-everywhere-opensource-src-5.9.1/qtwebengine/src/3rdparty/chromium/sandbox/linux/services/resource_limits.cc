// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/resource_limits.h"

#include <sys/resource.h>
#include <sys/time.h>

#include <algorithm>

namespace sandbox {

// static
bool ResourceLimits::Lower(int resource, rlim_t limit) {
  struct rlimit old_rlimit;
  if (getrlimit(resource, &old_rlimit))
    return false;
  // Make sure we don't raise the existing limit.
  const struct rlimit new_rlimit = {std::min(old_rlimit.rlim_cur, limit),
                                    std::min(old_rlimit.rlim_max, limit)};
  int rc = setrlimit(resource, &new_rlimit);
  return rc == 0;
}

}  // namespace sandbox
