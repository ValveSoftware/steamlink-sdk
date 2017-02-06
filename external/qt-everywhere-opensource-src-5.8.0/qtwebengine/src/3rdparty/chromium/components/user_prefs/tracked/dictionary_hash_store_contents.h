// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_
#define COMPONENTS_USER_PREFS_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/hash_store_contents.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Implements HashStoreContents by storing MACs in a DictionaryValue. The
// DictionaryValue is presumed to be the contents of a PrefStore.
// RegisterProfilePrefs() may be used to register all of the preferences used by
// this object.
class DictionaryHashStoreContents : public HashStoreContents {
 public:
  // Constructs a DictionaryHashStoreContents that reads from and writes to
  // |storage|.
  explicit DictionaryHashStoreContents(base::DictionaryValue* storage);

  // Registers required preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // HashStoreContents implementation
  std::string hash_store_id() const override;
  void Reset() override;
  bool IsInitialized() const override;
  const base::DictionaryValue* GetContents() const override;
  std::unique_ptr<MutableDictionary> GetMutableContents() override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  base::DictionaryValue* storage_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryHashStoreContents);
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_
