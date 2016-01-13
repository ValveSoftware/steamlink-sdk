// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/key_system_names.h"

#include <string>

namespace media {

const char kClearKey[] = "org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";

static bool IsParentKeySystemOf(const std::string& parent_key_system,
                                const std::string& key_system) {
  std::string prefix = parent_key_system + '.';
  return key_system.substr(0, prefix.size()) == prefix;
}

bool IsExternalClearKey(const std::string& key_system) {
  return key_system == kExternalClearKey ||
         IsParentKeySystemOf(kExternalClearKey, key_system);
}

}  // namespace media
