// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_TRACKED_PREF_HASH_STORE_H_
#define COMPONENTS_PREFS_TRACKED_PREF_HASH_STORE_H_

#include <memory>

class HashStoreContents;
class PrefHashStoreTransaction;

// Holds the configuration and implementation used to calculate and verify
// preference MACs.
// TODO(gab): Rename this class as it is no longer a store.
class PrefHashStore {
 public:
  virtual ~PrefHashStore() {}

  // Returns a PrefHashStoreTransaction which can be used to perform a series
  // of operations on the hash store. |storage| MAY be used as the backing store
  // depending on the implementation. Therefore the HashStoreContents used for
  // related transactions should correspond to the same underlying data store.
  virtual std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      std::unique_ptr<HashStoreContents> storage) = 0;
};

#endif  // COMPONENTS_PREFS_TRACKED_PREF_HASH_STORE_H_
