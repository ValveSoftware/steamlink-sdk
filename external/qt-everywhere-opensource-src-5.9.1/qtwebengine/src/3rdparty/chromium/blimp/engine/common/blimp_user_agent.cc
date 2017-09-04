// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/common/blimp_user_agent.h"

#include <string>

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"

namespace blimp {
namespace engine {

namespace {

std::string client_os_info = "Linux; Android 5.1.1";

}  // namespace

std::string GetBlimpEngineUserAgent() {
  return content::BuildUserAgentFromOSAndProduct(
      client_os_info,
      version_info::GetProductNameAndVersionForUserAgent() + " Mobile");
}

void SetClientOSInfo(std::string os_version_info) {
  client_os_info = os_version_info;
}

}  // namespace engine
}  // namespace blimp
