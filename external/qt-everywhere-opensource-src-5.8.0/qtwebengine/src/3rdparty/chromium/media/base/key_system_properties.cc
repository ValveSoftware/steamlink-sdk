// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_properties.h"

#include "base/logging.h"

namespace media {

SupportedCodecs KeySystemProperties::GetSupportedSecureCodecs() const {
#if !defined(OS_ANDROID)
  NOTREACHED();
#endif
  return EME_CODEC_NONE;
}

bool KeySystemProperties::UseAesDecryptor() const {
  return false;
}

std::string KeySystemProperties::GetPepperType() const {
#if !defined(ENABLE_PEPPER_CDMS)
  NOTREACHED();
#endif
  return "";
}

}  // namespace media
