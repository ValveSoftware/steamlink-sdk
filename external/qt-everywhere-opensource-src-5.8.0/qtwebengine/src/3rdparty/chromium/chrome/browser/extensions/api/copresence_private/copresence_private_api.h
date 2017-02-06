// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace audio_modem {
class WhispernetClient;
}

namespace extensions {

class CopresencePrivateService final : public BrowserContextKeyedAPI {
 public:
  explicit CopresencePrivateService(content::BrowserContext* context);
  ~CopresencePrivateService() override;

  // Registers a client to receive events from Whispernet.
  const std::string
  RegisterWhispernetClient(audio_modem::WhispernetClient* client);

  // Gets the whispernet client by ID.
  audio_modem::WhispernetClient* GetWhispernetClient(const std::string& id);

  // Called from the whispernet_proxy extension when it has initialized.
  void OnWhispernetInitialized(bool success);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CopresencePrivateService>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<CopresencePrivateService>;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceRedirectedInIncognito = true;
  static const char* service_name() { return "CopresencePrivateService"; }

  bool initialized_;
  std::map<std::string, audio_modem::WhispernetClient*> whispernet_clients_;

  DISALLOW_COPY_AND_ASSIGN(CopresencePrivateService);
};

template<>
void BrowserContextKeyedAPIFactory<CopresencePrivateService>
    ::DeclareFactoryDependencies();

class CopresencePrivateSendFoundFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendFound",
                             COPRESENCEPRIVATE_SENDFOUND);

 protected:
  ~CopresencePrivateSendFoundFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendSamplesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendSamples",
                             COPRESENCEPRIVATE_SENDSAMPLES);

 protected:
  ~CopresencePrivateSendSamplesFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendDetectFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendDetect",
                             COPRESENCEPRIVATE_SENDDETECT);

 protected:
  ~CopresencePrivateSendDetectFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendInitializedFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendInitialized",
                             COPRESENCEPRIVATE_SENDINITIALIZED);

 protected:
  ~CopresencePrivateSendInitializedFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
