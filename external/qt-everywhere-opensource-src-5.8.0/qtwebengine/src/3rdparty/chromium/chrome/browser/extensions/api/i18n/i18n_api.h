// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class Profile;

namespace extensions {

class I18nGetAcceptLanguagesFunction : public ChromeSyncExtensionFunction {
  ~I18nGetAcceptLanguagesFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("i18n.getAcceptLanguages", I18N_GETACCEPTLANGUAGES)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
