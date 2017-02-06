// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_

#include "extensions/browser/api/audio/audio_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class AudioService;

class AudioAPI : public BrowserContextKeyedAPI, public AudioService::Observer {
 public:
  explicit AudioAPI(content::BrowserContext* context);
  ~AudioAPI() override;

  AudioService* GetService() const;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AudioAPI>* GetFactoryInstance();

  // AudioService::Observer implementation.
  void OnDeviceChanged() override;
  void OnLevelChanged(const std::string& id, int level) override;
  void OnMuteChanged(bool is_input, bool is_muted) override;
  void OnDevicesChanged(const DeviceInfoList& devices) override;

 private:
  friend class BrowserContextKeyedAPIFactory<AudioAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "AudioAPI";
  }

  content::BrowserContext* const browser_context_;
  AudioService* service_;
};

class AudioGetInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.getInfo", AUDIO_GETINFO);

 protected:
  ~AudioGetInfoFunction() override {}
  bool RunSync() override;
};

class AudioSetActiveDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setActiveDevices", AUDIO_SETACTIVEDEVICES);

 protected:
  ~AudioSetActiveDevicesFunction() override {}
  bool RunSync() override;
};

class AudioSetPropertiesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setProperties", AUDIO_SETPROPERTIES);

 protected:
  ~AudioSetPropertiesFunction() override {}
  bool RunSync() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_
