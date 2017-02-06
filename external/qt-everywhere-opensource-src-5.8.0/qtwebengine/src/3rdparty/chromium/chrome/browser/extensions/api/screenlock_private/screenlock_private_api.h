// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

class ScreenlockPrivateGetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.getLocked",
                             SCREENLOCKPRIVATE_GETLOCKED)
  ScreenlockPrivateGetLockedFunction();
  bool RunAsync() override;

 private:
  ~ScreenlockPrivateGetLockedFunction() override;
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateGetLockedFunction);
};

class ScreenlockPrivateSetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.setLocked",
                             SCREENLOCKPRIVATE_SETLOCKED)
  ScreenlockPrivateSetLockedFunction();
  bool RunAsync() override;

 private:
  ~ScreenlockPrivateSetLockedFunction() override;
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateSetLockedFunction);
};

class ScreenlockPrivateAcceptAuthAttemptFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.acceptAuthAttempt",
                             SCREENLOCKPRIVATE_ACCEPTAUTHATTEMPT)
  ScreenlockPrivateAcceptAuthAttemptFunction();
  bool RunSync() override;

 private:
  ~ScreenlockPrivateAcceptAuthAttemptFunction() override;
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateAcceptAuthAttemptFunction);
};

class ScreenlockPrivateEventRouter
    : public BrowserContextKeyedAPI,
      public proximity_auth::ScreenlockBridge::Observer {
 public:
  explicit ScreenlockPrivateEventRouter(content::BrowserContext* context);
  ~ScreenlockPrivateEventRouter() override;

  bool OnAuthAttempted(
      proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type,
      const std::string& value);

  // BrowserContextKeyedAPI
  static BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>*
  GetFactoryInstance();
  void Shutdown() override;

 private:
  friend class BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>;

  // proximity_auth::ScreenlockBridge::Observer
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // BrowserContextKeyedAPI
  static const char* service_name() {
    return "ScreenlockPrivateEventRouter";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value* arg);

  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_
