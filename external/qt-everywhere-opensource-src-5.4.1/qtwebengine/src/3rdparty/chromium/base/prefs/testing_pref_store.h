// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_TESTING_PREF_STORE_H_
#define BASE_PREFS_TESTING_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_value_map.h"

// |TestingPrefStore| is a preference store implementation that allows tests to
// explicitly manipulate the contents of the store, triggering notifications
// where appropriate.
class TestingPrefStore : public PersistentPrefStore {
 public:
  TestingPrefStore();

  // Overriden from PrefStore.
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE;
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual bool HasObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;
  virtual bool ReadOnly() const OVERRIDE;
  virtual PrefReadError GetReadError() const OVERRIDE;
  virtual PersistentPrefStore::PrefReadError ReadPrefs() OVERRIDE;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;

  // Marks the store as having completed initialization.
  void SetInitializationCompleted();

  // Used for tests to trigger notifications explicitly.
  void NotifyPrefValueChanged(const std::string& key);
  void NotifyInitializationCompleted();

  // Some convenience getters/setters.
  void SetString(const std::string& key, const std::string& value);
  void SetInteger(const std::string& key, int value);
  void SetBoolean(const std::string& key, bool value);

  bool GetString(const std::string& key, std::string* value) const;
  bool GetInteger(const std::string& key, int* value) const;
  bool GetBoolean(const std::string& key, bool* value) const;

  // Determines whether ReadPrefsAsync completes immediately. Defaults to false
  // (non-blocking). To block, invoke this with true (blocking) before the call
  // to ReadPrefsAsync. To unblock, invoke again with false (non-blocking) after
  // the call to ReadPrefsAsync.
  void SetBlockAsyncRead(bool block_async_read);

  // Getter and Setter methods for setting and getting the state of the
  // |TestingPrefStore|.
  virtual void set_read_only(bool read_only);
  void set_read_success(bool read_success);
  void set_read_error(PersistentPrefStore::PrefReadError read_error);
  bool committed() { return committed_; }

 protected:
  virtual ~TestingPrefStore();

 private:
  // Stores the preference values.
  PrefValueMap prefs_;

  // Flag that indicates if the PrefStore is read-only
  bool read_only_;

  // The result to pass to PrefStore::Observer::OnInitializationCompleted
  bool read_success_;

  // The result to return from ReadPrefs or ReadPrefsAsync.
  PersistentPrefStore::PrefReadError read_error_;

  // Whether a call to ReadPrefsAsync should block.
  bool block_async_read_;

  // Whether there is a pending call to ReadPrefsAsync.
  bool pending_async_read_;

  // Whether initialization has been completed.
  bool init_complete_;

  // Whether the store contents have been committed to disk since the last
  // mutation.
  bool committed_;

  scoped_ptr<ReadErrorDelegate> error_delegate_;
  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefStore);
};

#endif  // BASE_PREFS_TESTING_PREF_STORE_H_
