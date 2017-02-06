// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_USER_AGENT_H_
#define BLIMP_CLIENT_APP_USER_AGENT_H_

#include <string>

namespace blimp {

/**
 * Builds a User-agent compatible string that describes the OS and CPU type.
 * e.g. Linux; Android 5.1.1; Nexus 6 Build/LMY49C
 */
std::string GetOSVersionInfoForUserAgent();

}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_USER_AGENT_H_
