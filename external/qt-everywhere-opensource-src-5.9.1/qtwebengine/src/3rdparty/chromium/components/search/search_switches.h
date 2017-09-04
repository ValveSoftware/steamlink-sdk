// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_SWITCHES_H_
#define COMPONENTS_SEARCH_SEARCH_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

#if defined(OS_ANDROID)
extern const char kEnableEmbeddedSearchAPI[];
extern const char kPrefetchSearchResults[];
#endif

}  // namespace switches

#endif  // COMPONENTS_SEARCH_SEARCH_SWITCHES_H_
