// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_ERROR_REPORTER_H_
#define HEADLESS_PUBLIC_UTIL_ERROR_REPORTER_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "headless/public/headless_export.h"

namespace headless {

// Tracks errors which are encountered while parsing client API types.
class HEADLESS_EXPORT ErrorReporter {
 public:
  ErrorReporter();
  ~ErrorReporter();

  // Enter a new nested parsing context. It will initially have a null name.
  void Push();

  // Leave the current parsing context, returning to the previous one.
  void Pop();

  // Set the name of the current parsing context. |name| must be a string with
  // application lifetime.
  void SetName(const char* name);

  // Report an error in the current parsing context.
  void AddError(base::StringPiece description);

  // Returns true if any errors have been reported so far.
  bool HasErrors() const;

  // Returns a list of reported errors.
  const std::vector<std::string>& errors() const { return errors_; }

 private:
  std::vector<const char*> path_;
  std::vector<std::string> errors_;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_ERROR_REPORTER_H_
