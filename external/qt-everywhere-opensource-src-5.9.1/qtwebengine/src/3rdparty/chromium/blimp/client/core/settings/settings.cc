// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "blimp/client/core/settings/settings_observer.h"
#include "blimp/client/core/settings/settings_prefs.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "components/prefs/pref_registry_simple.h"

namespace blimp {
namespace client {
namespace {

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    boolean_switch_map[] = {
        {blimp::switches::kEnableBlimp, prefs::kBlimpEnabled, true},
        {blimp::switches::kDownloadWholeDocument, prefs::kRecordWholeDocument,
         true},
};

}  // namespace

Settings::Settings(PrefService* local_state)
    : local_state_(local_state), show_network_stats_(false) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kBlimpEnabled,
      base::Bind(&Settings::OnPreferenceChanged, base::Unretained(this),
                 local_state, prefs::kBlimpEnabled));
  pref_change_registrar_.Add(
      prefs::kRecordWholeDocument,
      base::Bind(&Settings::OnPreferenceChanged, base::Unretained(this),
                 local_state, prefs::kRecordWholeDocument));
}

Settings::~Settings() = default;

void Settings::AddObserver(SettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void Settings::RemoveObserver(SettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Settings::SetShowNetworkStats(bool enable) {
  if (show_network_stats_ == enable)
    return;

  show_network_stats_ = enable;
  for (auto& observer : observers_)
    observer.OnShowNetworkStatsChanged(show_network_stats_);
}

void Settings::SetEnableBlimpMode(bool enable) {
  local_state_->SetBoolean(prefs::kBlimpEnabled, enable);
}

void Settings::SetRecordWholeDocument(bool enable) {
  local_state_->SetBoolean(prefs::kRecordWholeDocument, enable);
}

void Settings::OnPreferenceChanged(PrefService* service,
                                   const std::string& pref_name) {
  if (pref_name == prefs::kBlimpEnabled) {
    for (auto& observer : observers_) {
      observer.OnBlimpModeEnabled(service->GetBoolean(pref_name));
      observer.OnRestartRequired();
    }
  } else if (pref_name == prefs::kRecordWholeDocument) {
    for (auto& observer : observers_)
      observer.OnRecordWholeDocumentChanged(service->GetBoolean(pref_name));
  }
}

bool Settings::IsBlimpEnabled() {
  return local_state_->GetBoolean(prefs::kBlimpEnabled);
}

bool Settings::IsRecordWholeDocument() {
  return local_state_->GetBoolean(prefs::kRecordWholeDocument);
}

// static
void Settings::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kBlimpEnabled, false);
  registry->RegisterBooleanPref(prefs::kRecordWholeDocument, false);
}

// static
void Settings::ApplyBlimpSwitches(CommandLinePrefStore* store) {
  store->ApplyBooleanSwitches(boolean_switch_map,
                              arraysize(boolean_switch_map));
}

}  // namespace client
}  // namespace blimp
