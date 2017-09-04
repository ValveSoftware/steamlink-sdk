// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_common.h"

#include <stdint.h>

namespace search_provider_logos {

const int64_t kMaxTimeToLiveMS = INT64_C(30 * 24 * 60 * 60 * 1000);  // 30 days

LogoMetadata::LogoMetadata() : can_show_after_expiration(false) {}
LogoMetadata::LogoMetadata(const LogoMetadata& other) = default;
LogoMetadata::~LogoMetadata() {}

EncodedLogo::EncodedLogo() {}
EncodedLogo::EncodedLogo(const EncodedLogo& other) = default;
EncodedLogo::~EncodedLogo() {}

Logo::Logo() {}
Logo::~Logo() {}

}  // namespace search_provider_logos
