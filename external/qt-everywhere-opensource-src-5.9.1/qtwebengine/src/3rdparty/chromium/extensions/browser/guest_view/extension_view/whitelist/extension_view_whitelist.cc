// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_view/whitelist/extension_view_whitelist.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"

namespace extensions {

namespace {

// =============================================================================
//
// ADDING NEW EXTENSIONS REQUIRES APPROVAL from chrome-eng-review@google.com
//
// =============================================================================

const char* const kWhitelist[] = {
    "pemeknaakobkocgmimdeamlcklioagkh",  // Used in browser tests
    "dppcjffonoklmpdmljnpdojmoaefcabf",  // Used in browser tests
    "pkedcjkdefgpdelpbcmbmeomcjbeemfm",  // http://crbug.com/574889
    "ekpaaapppgpmolpcldedioblbkmijaca",  // http://crbug.com/552208
    "lhkfccafpkdlaodkicmokbmfapjadkij",  // http://crbug.com/552208
    "ibiljbkambkbohapfhoonkcpcikdglop",  // http://crbug.com/552208
    "enhhojjnijigcajfphajepfemndkmdlo",  // http://crbug.com/552208
};

}  // namespace

// static
bool IsExtensionIdWhitelisted(const std::string& extension_id) {
  for (size_t i = 0; i < arraysize(kWhitelist); ++i) {
    if (extension_id == kWhitelist[i])
      return true;
  }

  return false;
}

}  // namespace extensions
