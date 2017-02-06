// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_GET_CLIENT_TOKEN_H_
#define BLIMP_COMMON_GET_CLIENT_TOKEN_H_

#include <string>

#include "base/command_line.h"
#include "blimp/common/blimp_common_export.h"

namespace blimp {

// Gets the client token from the file provided by the command line. If a read
// does not succeed, or the switch is malformed, an empty string is returned.
BLIMP_COMMON_EXPORT std::string GetClientToken(
    const base::CommandLine& cmd_line);

}  // namespace blimp

#endif  // BLIMP_COMMON_GET_CLIENT_TOKEN_H_
