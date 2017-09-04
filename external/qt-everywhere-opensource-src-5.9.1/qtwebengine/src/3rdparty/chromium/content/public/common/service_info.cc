// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/service_info.h"

#include "base/callback.h"
#include "services/service_manager/public/cpp/service.h"

namespace content {

ServiceInfo::ServiceInfo() {}
ServiceInfo::ServiceInfo(const ServiceInfo& other) = default;
ServiceInfo::~ServiceInfo() {}

}  // namespace content
