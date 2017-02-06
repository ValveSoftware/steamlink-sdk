// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/chrome_direct_setting_api.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {
namespace chromedirectsetting {

const char kOnPrefChangeFormat[] =
    "types.private.ChromeDirectSetting.%s.onChange";

class PreferenceWhitelist {
 public:
  PreferenceWhitelist() {
    // Note: DO NOT add any setting here that does not have a UI element in
    // chrome://settings unless you write a component extension that is always
    // installed. Otherwise, users may install your extension, the extension may
    // toggle settings, and after the extension has been disabled/uninstalled
    // the toggled setting remains in place. See http://crbug.com/164227#c157 .
    // The following settings need to be checked and probably removed. See
    // http://crbug.com/164227#c157 .
    whitelist_.insert("easy_unlock.proximity_required");
  }

  ~PreferenceWhitelist() {}

  bool IsPreferenceOnWhitelist(const std::string& pref_key){
    return whitelist_.find(pref_key) != whitelist_.end();
  }

  void RegisterEventListeners(
      Profile* profile,
      EventRouter::Observer* observer) {
    for (base::hash_set<std::string>::iterator iter = whitelist_.begin();
         iter != whitelist_.end();
         iter++) {
      std::string event_name = base::StringPrintf(
          kOnPrefChangeFormat,
          (*iter).c_str());
      EventRouter::Get(profile)->RegisterObserver(observer, event_name);
    }
  }

  void RegisterPropertyListeners(
      Profile* profile,
      PrefChangeRegistrar* registrar,
      const base::Callback<void(const std::string&)>& callback) {
    for (base::hash_set<std::string>::iterator iter = whitelist_.begin();
         iter != whitelist_.end();
         iter++) {
      const char* pref_key = (*iter).c_str();
      std::string event_name = base::StringPrintf(
          kOnPrefChangeFormat,
          pref_key);
      registrar->Add(pref_key, callback);
    }
  }

 private:
  base::hash_set<std::string> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceWhitelist);
};

base::LazyInstance<PreferenceWhitelist> preference_whitelist =
    LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ChromeDirectSettingAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

ChromeDirectSettingAPI::ChromeDirectSettingAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  preference_whitelist.Get().RegisterEventListeners(profile_, this);
}

ChromeDirectSettingAPI::~ChromeDirectSettingAPI() {}

// KeyedService implementation.
void ChromeDirectSettingAPI::Shutdown() {}

// BrowserContextKeyedAPI implementation.
BrowserContextKeyedAPIFactory<ChromeDirectSettingAPI>*
ChromeDirectSettingAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// EventRouter::Observer implementation.
void ChromeDirectSettingAPI::OnListenerAdded(const EventListenerInfo& details) {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  registrar_.Init(profile_->GetPrefs());
  preference_whitelist.Get().RegisterPropertyListeners(
      profile_,
      &registrar_,
      base::Bind(&ChromeDirectSettingAPI::OnPrefChanged,
                 base::Unretained(this),
                 registrar_.prefs()));
}

bool ChromeDirectSettingAPI::IsPreferenceOnWhitelist(
    const std::string& pref_key) {
  return preference_whitelist.Get().IsPreferenceOnWhitelist(pref_key);
}

ChromeDirectSettingAPI* ChromeDirectSettingAPI::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ChromeDirectSettingAPI>::Get(context);
}

// BrowserContextKeyedAPI implementation.
const char* ChromeDirectSettingAPI::service_name() {
  return "ChromeDirectSettingAPI";
}

void ChromeDirectSettingAPI::OnPrefChanged(
    PrefService* pref_service, const std::string& pref_key) {
  std::string event_name = base::StringPrintf(kOnPrefChangeFormat,
                                              pref_key.c_str());
  EventRouter* router = EventRouter::Get(profile_);
  if (router && router->HasEventListener(event_name)) {
    const PrefService::Preference* preference =
        profile_->GetPrefs()->FindPreference(pref_key.c_str());
    const base::Value* value = preference->GetValue();

    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
    result->Set(preference_api_constants::kValue, value->DeepCopy());
    base::ListValue args;
    args.Append(std::move(result));

    for (const scoped_refptr<const extensions::Extension>& extension :
         ExtensionRegistry::Get(profile_)->enabled_extensions()) {
      const std::string& extension_id = extension->id();
      if (router->ExtensionHasEventListener(extension_id, event_name)) {
        std::unique_ptr<base::ListValue> args_copy(args.DeepCopy());
        // TODO(kalman): Have a histogram value for each pref type.
        // This isn't so important for the current use case of these
        // histograms, which is to track which event types are waking up event
        // pages, or which are delivered to persistent background pages. Simply
        // "a setting changed" is enough detail for that. However if we try to
        // use these histograms for any fine-grained logic (like removing the
        // string event name altogether), or if we discover this event is
        // firing a lot and want to understand that better, then this will need
        // to change.
        events::HistogramValue histogram_value =
            events::TYPES_PRIVATE_CHROME_DIRECT_SETTING_ON_CHANGE;
        std::unique_ptr<Event> event(
            new Event(histogram_value, event_name, std::move(args_copy)));
        router->DispatchEventToExtension(extension_id, std::move(event));
      }
    }
  }
}

}  // namespace chromedirectsetting
}  // namespace extensions
