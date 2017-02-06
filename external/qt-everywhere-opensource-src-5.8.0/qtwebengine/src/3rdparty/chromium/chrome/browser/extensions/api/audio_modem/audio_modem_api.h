// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUDIO_MODEM_AUDIO_MODEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUDIO_MODEM_AUDIO_MODEM_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/common/extensions/api/audio_modem.h"
#include "components/audio_modem/public/modem.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

// Implementation of the chrome.audioModem API.
class AudioModemAPI final : public BrowserContextKeyedAPI {
 public:
  // Default constructor.
  explicit AudioModemAPI(content::BrowserContext* context);

  // Testing constructor: pass in dependencies.
  AudioModemAPI(
      content::BrowserContext* context,
      std::unique_ptr<audio_modem::WhispernetClient> whispernet_client,
      std::unique_ptr<audio_modem::Modem> modem);

  ~AudioModemAPI() override;

  // Starts transmitting a token, and returns the associated API status.
  // Fails if another app is already transmitting the same AudioType.
  api::audio_modem::Status StartTransmit(
      const std::string& app_id,
      const api::audio_modem::RequestParams& params,
      const std::string& token);

  // Stops an in-progress transmit, and returns the associated API status.
  // Fails if the specified app is not currently transmitting.
  api::audio_modem::Status StopTransmit(const std::string& app_id,
                                        audio_modem::AudioType audio_type);

  // Starts receiving for the specified app.
  // Multiple apps may receive the same AudioType simultaneously.
  void StartReceive(const std::string& app_id,
                    const api::audio_modem::RequestParams& params);

  // Stops receiving for the specified app, and returns the associated
  // API status. Fails if that app is not currently receiving.
  api::audio_modem::Status StopReceive(const std::string& app_id,
                                       audio_modem::AudioType audio_type);

  bool init_failed() const { return init_failed_; }

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AudioModemAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<AudioModemAPI>;

  void WhispernetInitComplete(bool success);
  void TokensReceived(const std::vector<audio_modem::AudioToken>& tokens);

  content::BrowserContext* const browser_context_;
  std::unique_ptr<audio_modem::WhispernetClient> whispernet_client_;
  std::unique_ptr<audio_modem::Modem> modem_;
  bool init_failed_;

  // IDs for the currently transmitting app (if any), indexed by AudioType.
  std::string transmitters_[2];

  // Timeouts for the currently active transmits, indexed by AudioType.
  base::OneShotTimer transmit_timers_[2];

  // Maps of currently receiving app ID => timeouts. Indexed by AudioType.
  // We own all of these pointers. Do not remove them without calling delete.
  std::map<std::string, base::OneShotTimer*> receive_timers_[2];

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const char* service_name() { return "AudioModemAPI"; }

  DISALLOW_COPY_AND_ASSIGN(AudioModemAPI);
};

template<>
void BrowserContextKeyedAPIFactory<AudioModemAPI>::DeclareFactoryDependencies();

class AudioModemTransmitFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audioModem.transmit", AUDIOMODEM_TRANSMIT);

 protected:
  ~AudioModemTransmitFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class AudioModemStopTransmitFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audioModem.stopTransmit",
                             AUDIOMODEM_STOPTRANSMIT);

 protected:
  ~AudioModemStopTransmitFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class AudioModemReceiveFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audioModem.receive", AUDIOMODEM_RECEIVE);

 protected:
  ~AudioModemReceiveFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class AudioModemStopReceiveFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audioModem.stopReceive", AUDIOMODEM_STOPRECEIVE);

 protected:
  ~AudioModemStopReceiveFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUDIO_MODEM_AUDIO_MODEM_API_H_
