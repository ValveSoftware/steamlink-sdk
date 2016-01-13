// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_util.h"

#include <string>

namespace google_apis {
namespace util {

namespace {

// etag matching header.
const char kIfMatchHeaderPrefix[] = "If-Match: ";

}  // namespace

const char kIfMatchAllHeader[] = "If-Match: *";

std::string GenerateIfMatchHeader(const std::string& etag) {
  return etag.empty() ? kIfMatchAllHeader : (kIfMatchHeaderPrefix + etag);
}

}  // namespace util
}  // namespace google_apis
