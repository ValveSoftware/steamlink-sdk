// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class ExperienceSamplingPrivateGetBrowserInfoFunction
    : public ChromeAsyncExtensionFunction {
 protected:
  ~ExperienceSamplingPrivateGetBrowserInfoFunction() override {}

  // ExtensionFuction:
  bool RunAsync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("experienceSamplingPrivate.getBrowserInfo",
                             EXPERIENCESAMPLINGPRIVATE_GETBROWSERINFO);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_PRIVATE_API_H_
