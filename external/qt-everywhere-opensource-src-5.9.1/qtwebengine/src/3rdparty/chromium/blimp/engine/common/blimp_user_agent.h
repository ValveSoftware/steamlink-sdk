// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_COMMON_BLIMP_USER_AGENT_H_
#define BLIMP_ENGINE_COMMON_BLIMP_USER_AGENT_H_

#include <string>

#include "content/public/common/content_client.h"

namespace blimp {
namespace engine {

std::string GetBlimpEngineUserAgent();
void SetClientOSInfo(std::string client_os_info);

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_COMMON_BLIMP_USER_AGENT_H_
