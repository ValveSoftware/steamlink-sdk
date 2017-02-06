// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/browser/browser_api.h"

#include "chrome/browser/extensions/extension_tab_util.h"

namespace extensions {
namespace api {

BrowserOpenTabFunction::~BrowserOpenTabFunction() {
}

bool BrowserOpenTabFunction::RunSync() {
  std::unique_ptr<browser::OpenTab::Params> params(
      browser::OpenTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionTabUtil::OpenTabParams options;
  options.create_browser_if_needed = true;
  options.url.reset(new std::string(params->options.url));

  std::string error;
  std::unique_ptr<base::DictionaryValue> result(
      ExtensionTabUtil::OpenTab(this, options, &error));
  if (!result) {
    SetError(error);
    return false;
  }

  return true;
}

}  // namespace api
}  // namespace extensions
