// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_

#include <memory>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class GcdPrivateAPIImpl;

class GcdPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit GcdPrivateAPI(content::BrowserContext* context);
  ~GcdPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GcdPrivateAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<GcdPrivateAPI>;
  friend class GcdPrivateAPIImpl;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "GcdPrivateAPI"; }

  std::unique_ptr<GcdPrivateAPIImpl> impl_;
};

class GcdPrivateGetDeviceInfoFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getDeviceInfo",
                             GCDPRIVATE_GETDEVICEINFO)

  GcdPrivateGetDeviceInfoFunction();

 protected:
  ~GcdPrivateGetDeviceInfoFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnSessionInitialized(
      int session_id,
      api::gcd_private::Status status,
      const base::DictionaryValue& info);
};

class GcdPrivateCreateSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.createSession",
                             GCDPRIVATE_ESTABLISHSESSION)

  GcdPrivateCreateSessionFunction();

 protected:
  ~GcdPrivateCreateSessionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnSessionInitialized(
      int session_id,
      api::gcd_private::Status status,
      const base::DictionaryValue& info);
};

class GcdPrivateStartPairingFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.startPairing", GCDPRIVATE_STARTPAIRING)

  GcdPrivateStartPairingFunction();

 protected:
  ~GcdPrivateStartPairingFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnPairingStarted(api::gcd_private::Status status);
};

class GcdPrivateConfirmCodeFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.confirmCode", GCDPRIVATE_CONFIRMCODE)

  GcdPrivateConfirmCodeFunction();

 protected:
  ~GcdPrivateConfirmCodeFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnCodeConfirmed(api::gcd_private::Status status);
};

class GcdPrivateSendMessageFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.sendMessage", GCDPRIVATE_SENDMESSAGE)

  GcdPrivateSendMessageFunction();

 protected:
  ~GcdPrivateSendMessageFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnMessageSentCallback(api::gcd_private::Status status,
                             const base::DictionaryValue& value);
};

class GcdPrivateTerminateSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.terminateSession",
                             GCDPRIVATE_TERMINATESESSION)

  GcdPrivateTerminateSessionFunction();

 protected:
  ~GcdPrivateTerminateSessionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
