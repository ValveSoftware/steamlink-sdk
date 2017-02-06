// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mojo_application_info.h"

#include "base/callback.h"
#include "services/shell/public/cpp/shell_client.h"

namespace content {

MojoApplicationInfo::MojoApplicationInfo() {}

MojoApplicationInfo::MojoApplicationInfo(const MojoApplicationInfo& other)
    = default;

MojoApplicationInfo::~MojoApplicationInfo() {}

}  // namespace content
