// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/api/copresence/copresence_translations.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/copresence.h"
#include "components/copresence/public/copresence_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class ChromeWhispernetClient;

namespace audio_modem {
class WhispernetClient;
}

namespace copresence {
class CopresenceManager;
}

namespace gcm {
class GCMDriver;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class CopresenceService final : public BrowserContextKeyedAPI,
                                public copresence::CopresenceDelegate {
 public:
  explicit CopresenceService(content::BrowserContext* context);
  ~CopresenceService() override;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceHasOwnInstanceInIncognito = true;
  void Shutdown() override;

  // These accessors will always return an object (except during shutdown).
  // If the object doesn't exist, they will create one first.
  copresence::CopresenceManager* manager();

  // A registry containing the app id's associated with every subscription.
  SubscriptionToAppMap& apps_by_subscription_id() {
    return apps_by_subscription_id_;
  }

  std::string auth_token(const std::string& app_id) const;

  void set_api_key(const std::string& app_id,
                   const std::string& api_key);

  void set_auth_token(const std::string& app_id,
                      const std::string& token);

  // Delete all current copresence data, including stored device IDs.
  void ResetState();

  // Manager override for testing.
  void set_manager_for_testing(
      std::unique_ptr<copresence::CopresenceManager> manager);

  // Registers the preference for saving our device IDs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CopresenceService>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<CopresenceService>;

  // CopresenceDelegate implementation
  void HandleMessages(const std::string& app_id,
                      const std::string& subscription_id,
                      const std::vector<copresence::Message>& message) override;
  void HandleStatusUpdate(copresence::CopresenceStatus status) override;
  net::URLRequestContextGetter* GetRequestContext() const override;
  std::string GetPlatformVersionString() const override;
  std::string GetAPIKey(const std::string& app_id) const override;
  audio_modem::WhispernetClient* GetWhispernetClient() override;
  gcm::GCMDriver* GetGCMDriver() override;
  std::string GetDeviceId(bool authenticated) override;
  void SaveDeviceId(bool authenticated, const std::string& device_id) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CopresenceService"; }

  PrefService* GetPrefService();

  bool is_shutting_down_;
  content::BrowserContext* const browser_context_;

  std::map<std::string, std::string> apps_by_subscription_id_;

  std::map<std::string, std::string> api_keys_by_app_;
  std::map<std::string, std::string> auth_tokens_by_app_;

  std::unique_ptr<audio_modem::WhispernetClient> whispernet_client_;
  std::unique_ptr<copresence::CopresenceManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceService);
};

template <>
void BrowserContextKeyedAPIFactory<
    CopresenceService>::DeclareFactoryDependencies();

class CopresenceExecuteFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresence.execute", COPRESENCE_EXECUTE);

 protected:
  ~CopresenceExecuteFunction() override {}
  ExtensionFunction::ResponseAction Run() override;

 private:
  void SendResult(copresence::CopresenceStatus status);
};

// TODO(ckehoe): Remove this function.
class CopresenceSetApiKeyFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresence.setApiKey", COPRESENCE_SETAPIKEY);

 protected:
  ~CopresenceSetApiKeyFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresenceSetAuthTokenFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresence.setAuthToken",
                             COPRESENCE_SETAUTHTOKEN);

 protected:
  ~CopresenceSetAuthTokenFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_
