// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIME_UTIL_MIME_UTIL_H__
#define COMPONENTS_MIME_UTIL_MIME_UTIL_H__

#include <string>

namespace mime_util {

// Check to see if a particular MIME type is in the list of
// supported/recognized MIME types.
bool IsSupportedImageMimeType(const std::string& mime_type);
bool IsSupportedNonImageMimeType(const std::string& mime_type);
bool IsUnsupportedTextMimeType(const std::string& mime_type);
bool IsSupportedJavascriptMimeType(const std::string& mime_type);

// Convenience function.
bool IsSupportedMimeType(const std::string& mime_type);

}  // namespace mime_util

#endif  // COMPONENTS_MIME_UTIL_MIME_UTIL_H__
