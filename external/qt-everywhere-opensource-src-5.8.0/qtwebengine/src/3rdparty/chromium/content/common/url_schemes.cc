// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_schemes.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "content/common/savable_url_schemes.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

namespace {

}  // namespace

namespace content {

void RegisterContentSchemes(bool lock_schemes) {
  std::vector<url::SchemeWithType> additional_standard_schemes;
  std::vector<url::SchemeWithType> additional_referrer_schemes;
  std::vector<std::string> additional_savable_schemes;

  GetContentClient()->AddAdditionalSchemes(&additional_standard_schemes,
                                           &additional_referrer_schemes,
                                           &additional_savable_schemes);

  url::AddStandardScheme(kChromeDevToolsScheme, url::SCHEME_WITHOUT_PORT);
  url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITHOUT_PORT);
  url::AddStandardScheme(kGuestScheme, url::SCHEME_WITHOUT_PORT);

  for (const url::SchemeWithType& scheme : additional_standard_schemes)
    url::AddStandardScheme(scheme.scheme, scheme.type);

  for (const url::SchemeWithType& scheme : additional_referrer_schemes)
    url::AddReferrerScheme(scheme.scheme, scheme.type);

  // Prevent future modification of the scheme lists. This is to prevent
  // accidental creation of data races in the program. Add*Scheme aren't
  // threadsafe so must be called when GURL isn't used on any other thread. This
  // is really easy to mess up, so we say that all calls to Add*Scheme in Chrome
  // must be inside this function.
  if (lock_schemes)
    url::LockSchemeRegistries();

  // We rely on the above lock to protect this part from being invoked twice.
  if (!additional_savable_schemes.empty()) {
    const char* const* default_schemes = GetSavableSchemesInternal();
    const char* const* default_schemes_end = NULL;
    for (default_schemes_end = default_schemes; *default_schemes_end;
         ++default_schemes_end) {}
    const int default_schemes_count = default_schemes_end - default_schemes;

    int schemes = static_cast<int>(additional_savable_schemes.size());
    // The array, and the copied schemes won't be freed, but will remain
    // reachable.
    char **savable_schemes = new char*[schemes + default_schemes_count + 1];
    memcpy(savable_schemes,
           default_schemes,
           default_schemes_count * sizeof(default_schemes[0]));
    for (int i = 0; i < schemes; ++i) {
      savable_schemes[default_schemes_count + i] =
          base::strdup(additional_savable_schemes[i].c_str());
    }
    savable_schemes[default_schemes_count + schemes] = 0;

    SetSavableSchemes(savable_schemes);
  }
}

}  // namespace content
