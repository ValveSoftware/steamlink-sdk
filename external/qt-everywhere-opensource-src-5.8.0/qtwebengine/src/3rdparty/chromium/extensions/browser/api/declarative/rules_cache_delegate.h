// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class RulesRegistry;

// RulesCacheDelegate implements the part of the RulesRegistry which works on
// the UI thread. It should only be used on the UI thread.
// If |log_storage_init_delay| is set, the delay caused by loading and
// registering rules on initialization will be logged with UMA.
class RulesCacheDelegate {
 public:

  explicit RulesCacheDelegate(bool log_storage_init_delay);

  virtual ~RulesCacheDelegate();

  // Returns a key for the state store. The associated preference is a boolean
  // indicating whether there are some declarative rules stored in the rule
  // store.
  static std::string GetRulesStoredKey(const std::string& event_name,
                                       bool incognito);

  // Initialize the storage functionality.
  void Init(RulesRegistry* registry);

  void WriteToStorage(const std::string& extension_id,
                      std::unique_ptr<base::Value> value);

  base::WeakPtr<RulesCacheDelegate> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           DeclarativeRulesStored);
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           RulesStoredFlagMultipleRegistries);

  static const char kRulesStoredKey[];

  // Check if we are done reading all data from storage on startup, and notify
  // the RulesRegistry on its thread if so. The notification is delivered
  // exactly once.
  void CheckIfReady();

  // Schedules retrieving rules for already loaded extensions where
  // appropriate.
  void ReadRulesForInstalledExtensions();

  // Read/write a list of rules serialized to Values.
  void ReadFromStorage(const std::string& extension_id);
  void ReadFromStorageCallback(const std::string& extension_id,
                               std::unique_ptr<base::Value> value);

  // Check the preferences whether the extension with |extension_id| has some
  // rules stored on disk. If this information is not in the preferences, true
  // is returned as a safe default value.
  bool GetDeclarativeRulesStored(const std::string& extension_id) const;
  // Modify the preference to |rules_stored|.
  void SetDeclarativeRulesStored(const std::string& extension_id,
                                 bool rules_stored);

  content::BrowserContext* browser_context_;

  // The key under which rules are stored.
  std::string storage_key_;

  // The key under which we store whether the rules have been stored.
  std::string rules_stored_key_;

  // A set of extension IDs that have rules we are reading from storage.
  std::set<std::string> waiting_for_extensions_;

  // We measure the time spent on loading rules on init. The result is logged
  // with UMA once per each RulesCacheDelegate instance, unless in Incognito.
  base::Time storage_init_time_;
  bool log_storage_init_delay_;

  // Weak pointer to post tasks to the owning rules registry.
  base::WeakPtr<RulesRegistry> registry_;

  // The thread |registry_| lives on.
  content::BrowserThread::ID rules_registry_thread_;

  // We notified the RulesRegistry that the rules are loaded.
  bool notified_registry_;

  // Use this factory to generate weak pointers bound to the UI thread.
  base::WeakPtrFactory<RulesCacheDelegate> weak_ptr_factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
