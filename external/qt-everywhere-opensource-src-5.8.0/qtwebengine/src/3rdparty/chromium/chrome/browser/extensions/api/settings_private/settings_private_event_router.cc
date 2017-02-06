// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

SettingsPrivateEventRouter::SettingsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router) {
    event_router->RegisterObserver(
        this, api::settings_private::OnPrefsChanged::kEventName);
    StartOrStopListeningForPrefsChanges();
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  prefs_util_.reset(new PrefsUtil(profile));
  user_prefs_registrar_.Init(profile->GetPrefs());
  local_state_registrar_.Init(g_browser_process->local_state());
}

SettingsPrivateEventRouter::~SettingsPrivateEventRouter() {
  DCHECK(!listening_);
}

void SettingsPrivateEventRouter::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_) {
    cros_settings_subscription_map_.clear();
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    for (const auto& it : keys) {
      if (!prefs_util_->IsCrosSetting(it.first))
        FindRegistrarForPref(it.first)->Remove(it.first);
    }
  }
  listening_ = false;
}

void SettingsPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the PrefChangeRegistrars.
  StartOrStopListeningForPrefsChanges();
}

void SettingsPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the PrefChangeRegistrars if there are no
  // more listeners.
  StartOrStopListeningForPrefsChanges();
}

PrefChangeRegistrar* SettingsPrivateEventRouter::FindRegistrarForPref(
    const std::string& pref_name) {
  Profile* profile = Profile::FromBrowserContext(context_);
  if (prefs_util_->FindServiceForPref(pref_name) == profile->GetPrefs()) {
    return &user_prefs_registrar_;
  }
  return &local_state_registrar_;
}

void SettingsPrivateEventRouter::StartOrStopListeningForPrefsChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen = event_router->HasEventListener(
      api::settings_private::OnPrefsChanged::kEventName);

  if (should_listen && !listening_) {
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    for (const auto& it : keys) {
      std::string pref_name = it.first;
      if (prefs_util_->IsCrosSetting(pref_name)) {
#if defined(OS_CHROMEOS)
        std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
            chromeos::CrosSettings::Get()->AddSettingsObserver(
                pref_name.c_str(),
                base::Bind(&SettingsPrivateEventRouter::OnPreferenceChanged,
                           base::Unretained(this), pref_name));
        linked_ptr<chromeos::CrosSettings::ObserverSubscription> subscription(
            observer.release());
        cros_settings_subscription_map_.insert(
            make_pair(pref_name, subscription));
#endif
      } else {
        FindRegistrarForPref(it.first)
            ->Add(pref_name,
                  base::Bind(&SettingsPrivateEventRouter::OnPreferenceChanged,
                             base::Unretained(this)));
      }
    }
  } else if (!should_listen && listening_) {
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    for (const auto& it : keys) {
      if (prefs_util_->IsCrosSetting(it.first))
        cros_settings_subscription_map_.erase(it.first);
      else
        FindRegistrarForPref(it.first)->Remove(it.first);
    }
  }
  listening_ = should_listen;
}

void SettingsPrivateEventRouter::OnPreferenceChanged(
    const std::string& pref_name) {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::settings_private::OnPrefsChanged::kEventName)) {
    return;
  }

  std::unique_ptr<api::settings_private::PrefObject> pref_object =
      prefs_util_->GetPref(pref_name);

  std::vector<api::settings_private::PrefObject> prefs;
  if (pref_object)
    prefs.push_back(std::move(*pref_object));

  std::unique_ptr<base::ListValue> args(
      api::settings_private::OnPrefsChanged::Create(prefs));

  std::unique_ptr<Event> extension_event(new Event(
      events::SETTINGS_PRIVATE_ON_PREFS_CHANGED,
      api::settings_private::OnPrefsChanged::kEventName, std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

SettingsPrivateEventRouter* SettingsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new SettingsPrivateEventRouter(context);
}

}  // namespace extensions
