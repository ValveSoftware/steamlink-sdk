// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/dictionary_hash_store_contents.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/persistent_pref_store.h"

namespace {

const char kPreferenceMACs[] = "protection.macs";
const char kSuperMACPref[] = "protection.super_mac";

class MutablePreferenceMacDictionary
    : public HashStoreContents::MutableDictionary {
 public:
  explicit MutablePreferenceMacDictionary(base::DictionaryValue* storage);

  // MutableDictionary implementation
  base::DictionaryValue* operator->() override;

 private:
  base::DictionaryValue* storage_;

  DISALLOW_COPY_AND_ASSIGN(MutablePreferenceMacDictionary);
};

MutablePreferenceMacDictionary::MutablePreferenceMacDictionary(
    base::DictionaryValue* storage)
    : storage_(storage) {
}

base::DictionaryValue* MutablePreferenceMacDictionary::operator->() {
  base::DictionaryValue* mac_dictionary = NULL;

  if (!storage_->GetDictionary(kPreferenceMACs, &mac_dictionary)) {
    mac_dictionary = new base::DictionaryValue;
    storage_->Set(kPreferenceMACs, mac_dictionary);
  }

  return mac_dictionary;
}

}  // namespace

DictionaryHashStoreContents::DictionaryHashStoreContents(
    base::DictionaryValue* storage)
    : storage_(storage) {
}

// static
void DictionaryHashStoreContents::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kPreferenceMACs);
  registry->RegisterStringPref(kSuperMACPref, std::string());
}

std::string DictionaryHashStoreContents::hash_store_id() const {
  return "";
}

void DictionaryHashStoreContents::Reset() {
  storage_->Remove(kPreferenceMACs, NULL);
}

bool DictionaryHashStoreContents::IsInitialized() const {
  return storage_->GetDictionary(kPreferenceMACs, NULL);
}

const base::DictionaryValue* DictionaryHashStoreContents::GetContents() const {
  const base::DictionaryValue* mac_dictionary = NULL;
  storage_->GetDictionary(kPreferenceMACs, &mac_dictionary);
  return mac_dictionary;
}

std::unique_ptr<HashStoreContents::MutableDictionary>
DictionaryHashStoreContents::GetMutableContents() {
  return std::unique_ptr<MutableDictionary>(
      new MutablePreferenceMacDictionary(storage_));
}

std::string DictionaryHashStoreContents::GetSuperMac() const {
  std::string super_mac_string;
  storage_->GetString(kSuperMACPref, &super_mac_string);
  return super_mac_string;
}

void DictionaryHashStoreContents::SetSuperMac(const std::string& super_mac) {
  storage_->SetString(kSuperMACPref, super_mac);
}
