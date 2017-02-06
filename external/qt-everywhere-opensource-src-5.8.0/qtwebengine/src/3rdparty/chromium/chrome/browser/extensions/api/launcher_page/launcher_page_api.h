// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LAUNCHER_PAGE_LAUNCHER_PAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LAUNCHER_PAGE_LAUNCHER_PAGE_API_H_

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace app_list {
class AppListSyncableService;
class AppListModel;
}

namespace extensions {

class LauncherPageAPI : public BrowserContextKeyedAPI {
 public:
  explicit LauncherPageAPI(content::BrowserContext* context);
  ~LauncherPageAPI() override;

  app_list::AppListSyncableService* GetService() const;
  static BrowserContextKeyedAPIFactory<LauncherPageAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<LauncherPageAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "LauncherPageAPI"; }

  static const bool kServiceHasOwnInstanceInIncognito = true;

  app_list::AppListSyncableService* service_;
};

class LauncherPagePushSubpageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("launcherPage.pushSubpage",
                             LAUNCHERPAGE_PUSHSUBPAGE);

  LauncherPagePushSubpageFunction();

 protected:
  ~LauncherPagePushSubpageFunction() override {}

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPagePushSubpageFunction);
};

class LauncherPageShowFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("launcherPage.show", LAUNCHERPAGE_SHOW);

  LauncherPageShowFunction();

 protected:
  ~LauncherPageShowFunction() override {}

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPageShowFunction);
};

class LauncherPageHideFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("launcherPage.hide", LAUNCHERPAGE_HIDE);

  LauncherPageHideFunction();

 protected:
  ~LauncherPageHideFunction() override {}

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPageHideFunction);
};

class LauncherPageSetEnabledFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("launcherPage.setEnabled",
                             LAUNCHERPAGE_SETENABLED);

  LauncherPageSetEnabledFunction();

 protected:
  ~LauncherPageSetEnabledFunction() override {}

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPageSetEnabledFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LAUNCHER_PAGE_LAUNCHER_PAGE_API_H_
