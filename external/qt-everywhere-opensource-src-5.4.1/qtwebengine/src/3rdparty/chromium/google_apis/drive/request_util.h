// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_REQUEST_UTIL_H_
#define GOOGLE_APIS_DRIVE_REQUEST_UTIL_H_

#include <string>

namespace google_apis {
namespace util {

// If-Match header which matches to all etags.
extern const char kIfMatchAllHeader[];

// Returns If-Match header string for |etag|.
// If |etag| is empty, the returned header should match any etag.
std::string GenerateIfMatchHeader(const std::string& etag);

}  // namespace util
}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_REQUEST_UTIL_H_
