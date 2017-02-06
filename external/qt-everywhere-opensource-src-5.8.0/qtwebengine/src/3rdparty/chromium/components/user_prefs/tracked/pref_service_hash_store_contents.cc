// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/pref_service_hash_store_contents.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Implements get-or-create of a dictionary value and holds a
// DictionaryPrefUpdate.
class PrefServiceMutableDictionary
    : public HashStoreContents::MutableDictionary {
 public:
  // Creates an instance that provides mutable access to a dictionary value
  // named |key| that is a child of |kProfilePreferenceHashes| in
  // |prefs|.
  PrefServiceMutableDictionary(const std::string& key,
                               PrefService* pref_service);

  // HashStoreContents::MutableDictionary implementation
  base::DictionaryValue* operator->() override;

 private:
  const std::string key_;
  DictionaryPrefUpdate update_;
};

PrefServiceMutableDictionary::PrefServiceMutableDictionary(
    const std::string& key,
    PrefService* pref_service)
    : key_(key),
      update_(pref_service,
              PrefServiceHashStoreContents::kProfilePreferenceHashes) {
  DCHECK(!key_.empty());
}

base::DictionaryValue* PrefServiceMutableDictionary::operator->() {
  base::DictionaryValue* dictionary = NULL;
  if (!update_->GetDictionaryWithoutPathExpansion(key_, &dictionary)) {
    dictionary = new base::DictionaryValue;
    update_->SetWithoutPathExpansion(key_, dictionary);
  }
  return dictionary;
}

}  // namespace

// static
const char PrefServiceHashStoreContents::kProfilePreferenceHashes[] =
    "profile.preference_hashes";

// static
const char PrefServiceHashStoreContents::kHashOfHashesDict[] = "hash_of_hashes";

// static
const char PrefServiceHashStoreContents::kStoreVersionsDict[] =
    "store_versions";

PrefServiceHashStoreContents::PrefServiceHashStoreContents(
    const std::string& hash_store_id,
    PrefService* pref_service)
    : hash_store_id_(hash_store_id), pref_service_(pref_service) {
  // TODO(erikwright): Remove in M40+.
  DictionaryPrefUpdate update(pref_service_, kProfilePreferenceHashes);
  update->RemovePath(kStoreVersionsDict, NULL);
}

// static
void PrefServiceHashStoreContents::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the top level dictionary to map profile names to dictionaries of
  // tracked preferences.
  registry->RegisterDictionaryPref(kProfilePreferenceHashes);
}

// static
void PrefServiceHashStoreContents::ResetAllPrefHashStores(
    PrefService* pref_service) {
  pref_service->ClearPref(kProfilePreferenceHashes);
}

std::string PrefServiceHashStoreContents::hash_store_id() const {
  return hash_store_id_;
}

void PrefServiceHashStoreContents::Reset() {
  DictionaryPrefUpdate update(pref_service_, kProfilePreferenceHashes);

  update->RemoveWithoutPathExpansion(hash_store_id_, NULL);

  // Remove this store's entry in the kHashOfHashesDict.
  base::DictionaryValue* hash_of_hashes_dict;
  if (update->GetDictionaryWithoutPathExpansion(kHashOfHashesDict,
                                                &hash_of_hashes_dict)) {
    hash_of_hashes_dict->RemoveWithoutPathExpansion(hash_store_id_, NULL);
    if (hash_of_hashes_dict->empty())
      update->RemovePath(kHashOfHashesDict, NULL);
  }

  if (update->empty())
    pref_service_->ClearPref(kProfilePreferenceHashes);
}

bool PrefServiceHashStoreContents::IsInitialized() const {
  const base::DictionaryValue* pref_hash_dicts =
      pref_service_->GetDictionary(kProfilePreferenceHashes);
  return pref_hash_dicts->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                            NULL);
}

const base::DictionaryValue* PrefServiceHashStoreContents::GetContents() const {
  const base::DictionaryValue* pref_hash_dicts =
      pref_service_->GetDictionary(kProfilePreferenceHashes);
  const base::DictionaryValue* hashes_dict = NULL;
  pref_hash_dicts->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                     &hashes_dict);
  return hashes_dict;
}

std::unique_ptr<HashStoreContents::MutableDictionary>
PrefServiceHashStoreContents::GetMutableContents() {
  return std::unique_ptr<HashStoreContents::MutableDictionary>(
      new PrefServiceMutableDictionary(hash_store_id_, pref_service_));
}

std::string PrefServiceHashStoreContents::GetSuperMac() const {
  const base::DictionaryValue* pref_hash_dicts =
      pref_service_->GetDictionary(kProfilePreferenceHashes);
  const base::DictionaryValue* hash_of_hashes_dict = NULL;
  std::string hash_of_hashes;
  if (pref_hash_dicts->GetDictionaryWithoutPathExpansion(
          kHashOfHashesDict, &hash_of_hashes_dict)) {
    hash_of_hashes_dict->GetStringWithoutPathExpansion(hash_store_id_,
                                                       &hash_of_hashes);
  }
  return hash_of_hashes;
}

void PrefServiceHashStoreContents::SetSuperMac(const std::string& super_mac) {
  PrefServiceMutableDictionary(kHashOfHashesDict, pref_service_)
      ->SetStringWithoutPathExpansion(hash_store_id_, super_mac);
}
