// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_
#define CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace extensions {
class Extension;

// Validates the 'requirements' extension manifest field. This is an
// asynchronous process that involves several threads, but the public interface
// of this class (including constructor and destructor) must only be used on
// the UI thread.
class RequirementsChecker {
 public:
  virtual ~RequirementsChecker() {}

  using RequirementsCheckedCallback =
      base::Callback<void(const std::vector<std::string>& /* requirements */)>;

  // The vector passed to the callback are any localized errors describing
  // requirement violations. If this vector is non-empty, requirements checking
  // failed. This should only be called once. |callback| will always be invoked
  // asynchronously on the UI thread. |callback| will only be called once, and
  // will be reset after called.
  virtual void Check(const scoped_refptr<const Extension>& extension,
                     const RequirementsCheckedCallback& callback) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_
