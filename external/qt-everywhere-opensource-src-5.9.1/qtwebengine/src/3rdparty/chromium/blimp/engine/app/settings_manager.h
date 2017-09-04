// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_SETTINGS_MANAGER_H_
#define BLIMP_ENGINE_APP_SETTINGS_MANAGER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "blimp/engine/app/engine_settings.h"
#include "blimp/net/blimp_message_processor.h"
#include "content/public/browser/render_view_host.h"

namespace blimp {
namespace engine {

// This class is owned by the BlimpBrowserMainParts and manages the
// |EngineSettings| that can be set on the engine.
class SettingsManager {
 public:
  // An observer interested in knowing when the engine settings have changed.
  class Observer {
   public:
    // Called when the WebPreferences have changed. See content::WebPreferences.
    virtual void OnWebPreferencesChanged() {}
  };

  SettingsManager();
  ~SettingsManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called to update the WebPreferences.
  void UpdateWebkitPreferences(content::WebPreferences* prefs);

  const EngineSettings& GetEngineSettings() const;
  void UpdateEngineSettings(const EngineSettings& settings);

 private:
  EngineSettings settings_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SettingsManager);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_SETTINGS_MANAGER_H_
