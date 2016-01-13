// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_profiler/power_data_provider.h"

namespace content {

scoped_ptr<PowerDataProvider> PowerDataProvider::Create() {
  return make_scoped_ptr<PowerDataProvider>(NULL);
}

}  // namespace content
