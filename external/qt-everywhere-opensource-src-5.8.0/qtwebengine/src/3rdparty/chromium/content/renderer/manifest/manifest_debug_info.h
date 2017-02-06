// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MANIFEST_MANIFEST_DEBUG_INFO_H_
#define CONTENT_RENDERER_MANIFEST_MANIFEST_DEBUG_INFO_H_

#include <string>
#include <vector>

namespace content {

// ManifestDebugInfo contains debug information for the parsed manifest.
// It is created upon parsing and is available along the Manifest itself
// via ManifestManager. Parsing errors can be generic and critical, critical
// errors result in parser failure.
struct ManifestDebugInfo {
  struct Error {
    Error(const std::string& message, bool critical, int line, int column)
        : message(message),
          critical(critical),
          line(line),
          column(column) {}
    std::string message;
    bool critical;
    int line;
    int column;
  };

  ManifestDebugInfo();
  ~ManifestDebugInfo();
  std::vector<Error> errors;
  std::string raw_data;
};

} // namespace content

#endif  // CONTENT_RENDERER_MANIFEST_MANIFEST_DEBUG_INFO_H_
