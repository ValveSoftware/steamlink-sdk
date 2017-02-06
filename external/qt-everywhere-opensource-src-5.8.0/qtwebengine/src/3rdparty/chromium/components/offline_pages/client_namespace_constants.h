// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CLIENT_NAMESPACE_CONSTANTS_H_
#define COMPONENTS_OFFLINE_PAGES_CLIENT_NAMESPACE_CONSTANTS_H_

#include "build/build_config.h"

namespace offline_pages {

// Any changes to these well-known namespaces should also be reflected in
// OfflinePagesNamespace (histograms.xml) for consistency.
extern const char kBookmarkNamespace[];
extern const char kLastNNamespace[];

// Currently used for fallbacks like tests.
extern const char kDefaultNamespace[];

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CLIENT_NAMESPACE_CONSTANTS_H_
