// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CRYPTOTOKEN_PRIVATE_CRYPTOTOKEN_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CRYPTOTOKEN_PRIVATE_CRYPTOTOKEN_PRIVATE_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/cryptotoken_private.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.cryptotokenPrivate API functions.

namespace extensions {
namespace api {

class CryptotokenPrivateCanOriginAssertAppIdFunction
    : public UIThreadExtensionFunction {
  public:
    CryptotokenPrivateCanOriginAssertAppIdFunction();
    DECLARE_EXTENSION_FUNCTION("cryptotokenPrivate.canOriginAssertAppId",
                               CRYPTOTOKENPRIVATE_CANORIGINASSERTAPPID)
  protected:
    ~CryptotokenPrivateCanOriginAssertAppIdFunction() override {}
    ResponseAction Run() override;

  private:
    ChromeExtensionFunctionDetails chrome_details_;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CRYPTOTOKEN_PRIVATE_CRYPTOTOKEN_PRIVATE_API_H_
