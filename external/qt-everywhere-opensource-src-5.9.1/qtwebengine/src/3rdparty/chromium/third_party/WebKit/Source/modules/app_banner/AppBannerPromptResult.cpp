// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/AppBannerPromptResult.h"

namespace blink {

AppBannerPromptResult::~AppBannerPromptResult() {}

String AppBannerPromptResult::outcome() const {
  switch (m_outcome) {
    case Outcome::Accepted:
      return "accepted";

    case Outcome::Dismissed:
      return "dismissed";
  }

  ASSERT_NOT_REACHED();
  return "";
}

AppBannerPromptResult::AppBannerPromptResult(const String& platform,
                                             Outcome outcome)
    : m_platform(platform), m_outcome(outcome) {}

}  // namespace blink
