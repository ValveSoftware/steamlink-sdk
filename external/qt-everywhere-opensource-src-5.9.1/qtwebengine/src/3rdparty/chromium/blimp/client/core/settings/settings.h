// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_H_
#define BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"

class CommandLinePrefStore;
class PrefRegistrySimple;

namespace blimp {
namespace client {

class SettingsObserver;

class Settings : public PrefObserver {
 public:
  explicit Settings(PrefService* local_state);
  virtual ~Settings();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void ApplyBlimpSwitches(CommandLinePrefStore* store);

  void AddObserver(SettingsObserver* observer);
  void RemoveObserver(SettingsObserver* observer);

  // Set whether or not Blimp mode is enabled.
  void SetEnableBlimpMode(bool enable);

  // Set record_whole_document_, and save it to the persistent storage.
  void SetRecordWholeDocument(bool enable);

  // Set show_network_stats_.
  void SetShowNetworkStats(bool enable);

  // PrefObserver implementation.
  void OnPreferenceChanged(PrefService* service,
                           const std::string& pref_name) override;

  // Whether or not Blimp mode is enabled.
  // Can be changed via: command line --enable-blimp, setting dialog.
  // Persisted across restarts: Yes.
  bool IsBlimpEnabled();

  // Whether or not the engine sends the whole webpage at once or pieces of it
  // based on the current viewport.
  // Can be changed via: command line --download-whole-document, setting dialog.
  // Persisted across restarts: Yes.
  bool IsRecordWholeDocument();

  // Whether or not to show the network stats.
  // Can be changed via: setting dialog.
  // Persisted across restarts: No.
  bool show_network_stats() { return show_network_stats_; }

 private:
  // The pref service used to store the persistent settings.
  PrefService* local_state_;

  PrefChangeRegistrar pref_change_registrar_;

  // A list of all the observers of the Blimp Settings.
  base::ObserverList<SettingsObserver> observers_;

  bool show_network_stats_;

  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_H_
