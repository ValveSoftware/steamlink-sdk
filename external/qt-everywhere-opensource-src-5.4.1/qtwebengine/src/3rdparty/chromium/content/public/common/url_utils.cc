// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_utils.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/savable_url_schemes.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

static size_t g_max_url_size = 2 * 1024 * 1024;

const char* const* GetSavableSchemes() {
  return GetSavableSchemesInternal();
}

bool HasWebUIScheme(const GURL& url) {
  return
#if !defined(OS_IOS)
         url.SchemeIs(kChromeDevToolsScheme) ||
#endif
         url.SchemeIs(kChromeUIScheme);
}

bool IsSavableURL(const GURL& url) {
  for (int i = 0; GetSavableSchemes()[i] != NULL; ++i) {
    if (url.SchemeIs(GetSavableSchemes()[i]))
      return true;
  }
  return false;
}

#if defined(OS_ANDROID)
void SetMaxURLChars(size_t max_chars) {
  // Check that it is not used by a multiprocesses embedder
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  CHECK(cmd->HasSwitch(switches::kSingleProcess));
  g_max_url_size = max_chars;
}
#endif

size_t GetMaxURLChars() {
  return g_max_url_size;
}

}  // namespace content
