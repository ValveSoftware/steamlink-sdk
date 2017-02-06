// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TOP_SITES_TOP_SITES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TOP_SITES_TOP_SITES_API_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/history/core/browser/history_types.h"

namespace extensions {

class TopSitesGetFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("topSites.get", TOPSITES_GET)

  TopSitesGetFunction();

 protected:
  ~TopSitesGetFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data);

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<TopSitesGetFunction> weak_ptr_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TOP_SITES_TOP_SITES_API_H_
