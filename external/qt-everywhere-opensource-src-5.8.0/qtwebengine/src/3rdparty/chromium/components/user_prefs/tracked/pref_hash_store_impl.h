// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_PREF_HASH_STORE_IMPL_H_
#define COMPONENTS_USER_PREFS_TRACKED_PREF_HASH_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/pref_hash_calculator.h"
#include "components/user_prefs/tracked/pref_hash_store.h"

class HashStoreContents;
class PrefHashStoreTransaction;

// Implements PrefHashStoreImpl by storing preference hashes in a
// HashStoreContents.
class PrefHashStoreImpl : public PrefHashStore {
 public:
  enum StoreVersion {
    // No hashes have been stored in this PrefHashStore yet.
    VERSION_UNINITIALIZED = 0,
    // The hashes in this PrefHashStore were stored before the introduction
    // of a version number and should be re-initialized.
    VERSION_PRE_MIGRATION = 1,
    // The hashes in this PrefHashStore were stored using the latest algorithm.
    VERSION_LATEST = 2,
  };

  // Constructs a PrefHashStoreImpl that calculates hashes using
  // |seed| and |device_id| and stores them in |contents|.
  //
  // The same |seed| and |device_id| must be used to load and validate
  // previously stored hashes in |contents|.
  PrefHashStoreImpl(const std::string& seed,
                    const std::string& device_id,
                    bool use_super_mac);

  ~PrefHashStoreImpl() override;

  // Provides an external HashStoreContents implementation to be used.
  // BeginTransaction() will ignore |storage| if this is provided.
  void set_legacy_hash_store_contents(
      std::unique_ptr<HashStoreContents> legacy_hash_store_contents);

  // Clears the contents of this PrefHashStore. |IsInitialized()| will return
  // false after this call.
  void Reset();

  // PrefHashStore implementation.
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      std::unique_ptr<HashStoreContents> storage) override;

 private:
  class PrefHashStoreTransactionImpl;

  const PrefHashCalculator pref_hash_calculator_;
  std::unique_ptr<HashStoreContents> legacy_hash_store_contents_;
  bool use_super_mac_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreImpl);
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_PREF_HASH_STORE_IMPL_H_
