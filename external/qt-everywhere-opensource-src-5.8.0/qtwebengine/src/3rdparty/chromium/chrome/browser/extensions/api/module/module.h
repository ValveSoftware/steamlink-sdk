// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {
class ExtensionPrefs;

namespace extension {
// Return the extension's update URL data, if any.
std::string GetUpdateURLData(const ExtensionPrefs* prefs,
                             const std::string& extension_id);
}  // namespace extension

class ExtensionSetUpdateUrlDataFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.setUpdateUrlData",
                             EXTENSION_SETUPDATEURLDATA)

 protected:
  ~ExtensionSetUpdateUrlDataFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ExtensionIsAllowedIncognitoAccessFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedIncognitoAccess",
                             EXTENSION_ISALLOWEDINCOGNITOACCESS)

 protected:
  ~ExtensionIsAllowedIncognitoAccessFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ExtensionIsAllowedFileSchemeAccessFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedFileSchemeAccess",
                             EXTENSION_ISALLOWEDFILESCHEMEACCESS)

 protected:
  ~ExtensionIsAllowedFileSchemeAccessFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_
