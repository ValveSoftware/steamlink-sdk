// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

namespace content {

PowerSaveBlocker::~PowerSaveBlocker() {}

// static
scoped_ptr<PowerSaveBlocker> PowerSaveBlocker::Create(
    PowerSaveBlockerType type,
    const std::string& reason) {
  return scoped_ptr<PowerSaveBlocker>(new PowerSaveBlockerImpl(type, reason));
}

}  // namespace content
