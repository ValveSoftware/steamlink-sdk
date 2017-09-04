// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_test_configuration.h"

namespace content {

ShellTestConfiguration::ShellTestConfiguration()
    : enable_pixel_dumping(true),
      allow_external_pages(false) {}

ShellTestConfiguration::~ShellTestConfiguration() {}

}  // namespace content
