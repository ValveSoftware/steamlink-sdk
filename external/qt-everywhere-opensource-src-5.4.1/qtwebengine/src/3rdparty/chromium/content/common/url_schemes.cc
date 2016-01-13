// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_schemes.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "content/common/savable_url_schemes.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

namespace {

void AddStandardSchemeHelper(const std::string& scheme) {
  url::AddStandardScheme(scheme.c_str());
}

}  // namespace

namespace content {

void RegisterContentSchemes(bool lock_standard_schemes) {
  std::vector<std::string> additional_standard_schemes;
  std::vector<std::string> additional_savable_schemes;
  GetContentClient()->AddAdditionalSchemes(&additional_standard_schemes,
                                           &additional_savable_schemes);

  url::AddStandardScheme(kChromeDevToolsScheme);
  url::AddStandardScheme(kChromeUIScheme);
  url::AddStandardScheme(kGuestScheme);
  url::AddStandardScheme(kMetadataScheme);
  std::for_each(additional_standard_schemes.begin(),
                additional_standard_schemes.end(),
                AddStandardSchemeHelper);

  // Prevent future modification of the standard schemes list. This is to
  // prevent accidental creation of data races in the program. AddStandardScheme
  // isn't threadsafe so must be called when GURL isn't used on any other
  // thread. This is really easy to mess up, so we say that all calls to
  // AddStandardScheme in Chrome must be inside this function.
  if (lock_standard_schemes)
    url::LockStandardSchemes();

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
