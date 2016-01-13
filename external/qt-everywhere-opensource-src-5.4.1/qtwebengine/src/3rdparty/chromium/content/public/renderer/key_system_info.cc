// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_info.h"

namespace content {

KeySystemInfo::KeySystemInfo(const std::string& key_system)
    : key_system(key_system),
      use_aes_decryptor(false) {
}

KeySystemInfo::~KeySystemInfo() {
}

}  // namespace content
