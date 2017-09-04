// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;
class TemplateURL;

namespace extensions {
class ExtensionRegistry;

class SettingsOverridesAPI : public BrowserContextKeyedAPI,
                             public ExtensionRegistryObserver {
 public:
  explicit SettingsOverridesAPI(content::BrowserContext* context);
  ~SettingsOverridesAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SettingsOverridesAPI>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SettingsOverridesAPI>;

  typedef std::set<scoped_refptr<const Extension> > PendingExtensions;

  // Wrappers around PreferenceAPI.
  void SetPref(const std::string& extension_id,
               const std::string& pref_key,
               base::Value* value);
  void UnsetPref(const std::string& extension_id,
                 const std::string& pref_key);

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // KeyedService implementation.
  void Shutdown() override;

  void OnTemplateURLsLoaded();

  void RegisterSearchProvider(const Extension* extension) const;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SettingsOverridesAPI"; }

  Profile* profile_;
  TemplateURLService* url_service_;

  // List of extensions waiting for the TemplateURLService to Load to
  // have search provider registered.
  PendingExtensions pending_extensions_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_sub_;

  DISALLOW_COPY_AND_ASSIGN(SettingsOverridesAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    SettingsOverridesAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
