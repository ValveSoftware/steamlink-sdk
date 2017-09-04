// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/savable_url_schemes.h"

#include <stdlib.h>

#include "content/public/common/url_constants.h"

namespace content {

namespace {

const char* const kDefaultSavableSchemes[] = {
  url::kHttpScheme,
  url::kHttpsScheme,
  url::kFileScheme,
  url::kFileSystemScheme,
  url::kFtpScheme,
  kChromeDevToolsScheme,
  kChromeUIScheme,
  url::kDataScheme,
  NULL
};

const char* const* g_savable_schemes = kDefaultSavableSchemes;

}  // namespace

const char* const* GetSavableSchemesInternal() {
  return g_savable_schemes;
}

void SetSavableSchemes(const char* const* savable_schemes) {
  g_savable_schemes = savable_schemes;
}

}  // namespace content
