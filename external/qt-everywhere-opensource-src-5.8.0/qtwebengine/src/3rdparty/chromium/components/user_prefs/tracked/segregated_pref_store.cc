// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/segregated_pref_store.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"

SegregatedPrefStore::AggregatingObserver::AggregatingObserver(
    SegregatedPrefStore* outer)
    : outer_(outer),
      failed_sub_initializations_(0),
      successful_sub_initializations_(0) {
}

void SegregatedPrefStore::AggregatingObserver::OnPrefValueChanged(
    const std::string& key) {
  // There is no need to tell clients about changes if they have not yet been
  // told about initialization.
  if (failed_sub_initializations_ + successful_sub_initializations_ < 2)
    return;

  FOR_EACH_OBSERVER(PrefStore::Observer, outer_->observers_,
                    OnPrefValueChanged(key));
}

void SegregatedPrefStore::AggregatingObserver::OnInitializationCompleted(
    bool succeeded) {
  if (succeeded)
    ++successful_sub_initializations_;
  else
    ++failed_sub_initializations_;

  DCHECK_LE(failed_sub_initializations_ + successful_sub_initializations_, 2);

  if (failed_sub_initializations_ + successful_sub_initializations_ == 2) {
    if (successful_sub_initializations_ == 2 && outer_->read_error_delegate_) {
      PersistentPrefStore::PrefReadError read_error = outer_->GetReadError();
      if (read_error != PersistentPrefStore::PREF_READ_ERROR_NONE)
        outer_->read_error_delegate_->OnError(read_error);
    }

    FOR_EACH_OBSERVER(
        PrefStore::Observer, outer_->observers_,
        OnInitializationCompleted(successful_sub_initializations_ == 2));
  }
}

SegregatedPrefStore::SegregatedPrefStore(
    const scoped_refptr<PersistentPrefStore>& default_pref_store,
    const scoped_refptr<PersistentPrefStore>& selected_pref_store,
    const std::set<std::string>& selected_pref_names)
    : default_pref_store_(default_pref_store),
      selected_pref_store_(selected_pref_store),
      selected_preference_names_(selected_pref_names),
      aggregating_observer_(this) {
  default_pref_store_->AddObserver(&aggregating_observer_);
  selected_pref_store_->AddObserver(&aggregating_observer_);
}

void SegregatedPrefStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SegregatedPrefStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SegregatedPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool SegregatedPrefStore::IsInitializationComplete() const {
  return default_pref_store_->IsInitializationComplete() &&
         selected_pref_store_->IsInitializationComplete();
}

bool SegregatedPrefStore::GetValue(const std::string& key,
                                   const base::Value** result) const {
  return StoreForKey(key)->GetValue(key, result);
}

void SegregatedPrefStore::SetValue(const std::string& key,
                                   std::unique_ptr<base::Value> value,
                                   uint32_t flags) {
  StoreForKey(key)->SetValue(key, std::move(value), flags);
}

void SegregatedPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  StoreForKey(key)->RemoveValue(key, flags);
}

bool SegregatedPrefStore::GetMutableValue(const std::string& key,
                                          base::Value** result) {
  return StoreForKey(key)->GetMutableValue(key, result);
}

void SegregatedPrefStore::ReportValueChanged(const std::string& key,
                                             uint32_t flags) {
  StoreForKey(key)->ReportValueChanged(key, flags);
}

void SegregatedPrefStore::SetValueSilently(const std::string& key,
                                           std::unique_ptr<base::Value> value,
                                           uint32_t flags) {
  StoreForKey(key)->SetValueSilently(key, std::move(value), flags);
}

bool SegregatedPrefStore::ReadOnly() const {
  return selected_pref_store_->ReadOnly() || default_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::GetReadError() const {
  PersistentPrefStore::PrefReadError read_error =
      default_pref_store_->GetReadError();
  if (read_error == PersistentPrefStore::PREF_READ_ERROR_NONE) {
    read_error = selected_pref_store_->GetReadError();
    // Ignore NO_FILE from selected_pref_store_.
    if (read_error == PersistentPrefStore::PREF_READ_ERROR_NO_FILE)
      read_error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  }
  return read_error;
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::ReadPrefs() {
  // Note: Both of these stores own PrefFilters which makes ReadPrefs
  // asynchronous. This is okay in this case as only the first call will be
  // truly asynchronous, the second call will then unblock the migration in
  // TrackedPreferencesMigrator and complete synchronously.
  default_pref_store_->ReadPrefs();
  PersistentPrefStore::PrefReadError selected_store_read_error =
      selected_pref_store_->ReadPrefs();
  DCHECK_NE(PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE,
            selected_store_read_error);

  return GetReadError();
}

void SegregatedPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  read_error_delegate_.reset(error_delegate);
  default_pref_store_->ReadPrefsAsync(NULL);
  selected_pref_store_->ReadPrefsAsync(NULL);
}

void SegregatedPrefStore::CommitPendingWrite() {
  default_pref_store_->CommitPendingWrite();
  selected_pref_store_->CommitPendingWrite();
}

void SegregatedPrefStore::SchedulePendingLossyWrites() {
  default_pref_store_->SchedulePendingLossyWrites();
  selected_pref_store_->SchedulePendingLossyWrites();
}

void SegregatedPrefStore::ClearMutableValues() {
  NOTIMPLEMENTED();
}

SegregatedPrefStore::~SegregatedPrefStore() {
  default_pref_store_->RemoveObserver(&aggregating_observer_);
  selected_pref_store_->RemoveObserver(&aggregating_observer_);
}

PersistentPrefStore* SegregatedPrefStore::StoreForKey(const std::string& key) {
  return (ContainsKey(selected_preference_names_, key)
              ? selected_pref_store_
              : default_pref_store_).get();
}

const PersistentPrefStore* SegregatedPrefStore::StoreForKey(
    const std::string& key) const {
  return (ContainsKey(selected_preference_names_, key)
              ? selected_pref_store_
              : default_pref_store_).get();
}
