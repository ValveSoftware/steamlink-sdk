// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BROWSER_BROWSER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BROWSER_BROWSER_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/browser.h"

namespace extensions {
namespace api {

class BrowserOpenTabFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browser.openTab", BROWSER_OPENTAB)

 protected:
  ~BrowserOpenTabFunction() override;

  bool RunSync() override;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BROWSER_BROWSER_API_H_
