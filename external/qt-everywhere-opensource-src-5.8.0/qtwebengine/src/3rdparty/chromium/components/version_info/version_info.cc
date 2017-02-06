// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/version_info.h"

#include "build/build_config.h"
#include "components/version_info/version_info_values.h"
#include "grit/components_strings.h"

#if defined(USE_UNOFFICIAL_VERSION_NUMBER)
#include "ui/base/l10n/l10n_util.h"  // nogncheck
#endif  // USE_UNOFFICIAL_VERSION_NUMBER

namespace version_info {

std::string GetProductNameAndVersionForUserAgent() {
  return "Chrome/" + GetVersionNumber();
}

std::string GetProductName() {
  return PRODUCT_NAME;
}

std::string GetVersionNumber() {
  return PRODUCT_VERSION;
}

std::string GetLastChange() {
  return LAST_CHANGE;
}

bool IsOfficialBuild() {
  return IS_OFFICIAL_BUILD;
}

std::string GetOSType() {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_IOS)
  return "iOS";
#elif defined(OS_MACOSX)
  return "Mac OS X";
#elif defined(OS_CHROMEOS)
# if defined(GOOGLE_CHROME_BUILD)
  return "Chrome OS";
# else
  return "Chromium OS";
# endif
#elif defined(OS_ANDROID)
  return "Android";
#elif defined(OS_LINUX)
  return "Linux";
#elif defined(OS_FREEBSD)
  return "FreeBSD";
#elif defined(OS_OPENBSD)
  return "OpenBSD";
#elif defined(OS_SOLARIS)
  return "Solaris";
#else
  return "Unknown";
#endif
}

std::string GetChannelString(Channel channel) {
  switch (channel) {
    case Channel::STABLE:
      return "stable";
      break;
    case Channel::BETA:
      return "beta";
      break;
    case Channel::DEV:
      return "dev";
      break;
    case Channel::CANARY:
      return "canary";
      break;
    case Channel::UNKNOWN:
      return "unknown";
      break;
  }
  return std::string();
}

std::string GetVersionStringWithModifier(const std::string& modifier) {
  std::string current_version;
  current_version += GetVersionNumber();
#if defined(USE_UNOFFICIAL_VERSION_NUMBER)
  current_version += " (";
  current_version += l10n_util::GetStringUTF8(IDS_VERSION_UI_UNOFFICIAL);
  current_version += " ";
  current_version += GetLastChange();
  current_version += " ";
  current_version += GetOSType();
  current_version += ")";
#endif  // USE_UNOFFICIAL_VERSION_NUMBER
  if (!modifier.empty())
    current_version += " " + modifier;
  return current_version;
}

}  // namespace version_info
