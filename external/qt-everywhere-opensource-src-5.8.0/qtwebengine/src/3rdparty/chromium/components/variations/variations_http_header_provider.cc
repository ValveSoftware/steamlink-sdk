// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/variations/proto/client_variations.pb.h"

namespace variations {

// static
VariationsHttpHeaderProvider* VariationsHttpHeaderProvider::GetInstance() {
  return base::Singleton<VariationsHttpHeaderProvider>::get();
}

std::string VariationsHttpHeaderProvider::GetClientDataHeader() {
  // Lazily initialize the header, if not already done, before attempting to
  // transmit it.
  InitVariationIDsCacheIfNeeded();

  std::string variation_ids_header_copy;
  {
    base::AutoLock scoped_lock(lock_);
    variation_ids_header_copy = variation_ids_header_;
  }
  return variation_ids_header_copy;
}

std::string VariationsHttpHeaderProvider::GetVariationsString() {
  InitVariationIDsCacheIfNeeded();

  // Construct a space-separated string with leading and trailing spaces from
  // the variations set. Note: The ids in it will be in sorted order per the
  // std::set contract.
  std::string ids_string = " ";
  {
    base::AutoLock scoped_lock(lock_);
    for (VariationID id : GetAllVariationIds()) {
      ids_string.append(base::IntToString(id));
      ids_string.push_back(' ');
    }
  }
  return ids_string;
}

bool VariationsHttpHeaderProvider::SetDefaultVariationIds(
    const std::string& variation_ids) {
  default_variation_ids_set_.clear();
  default_trigger_id_set_.clear();
  for (const base::StringPiece& entry : base::SplitStringPiece(
           variation_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (entry.empty()) {
      default_variation_ids_set_.clear();
      default_trigger_id_set_.clear();
      return false;
    }
    bool trigger_id =
        base::StartsWith(entry, "t", base::CompareCase::SENSITIVE);
    // Remove the "t" prefix if it's there.
    base::StringPiece trimmed_entry = trigger_id ? entry.substr(1) : entry;

    int variation_id = 0;
    if (!base::StringToInt(trimmed_entry, &variation_id)) {
      default_variation_ids_set_.clear();
      default_trigger_id_set_.clear();
      return false;
    }
    if (trigger_id)
      default_trigger_id_set_.insert(variation_id);
    else
      default_variation_ids_set_.insert(variation_id);
  }
  return true;
}

void VariationsHttpHeaderProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);

  // Stop observing field trials so that it can be restarted when this is
  // re-inited. Note: This is a no-op if this is not currently observing.
  base::FieldTrialList::RemoveObserver(this);
  variation_ids_cache_initialized_ = false;
}

VariationsHttpHeaderProvider::VariationsHttpHeaderProvider()
    : variation_ids_cache_initialized_(false) {}

VariationsHttpHeaderProvider::~VariationsHttpHeaderProvider() {}

void VariationsHttpHeaderProvider::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  VariationID new_id =
      GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_name, group_name);
  VariationID new_trigger_id = GetGoogleVariationID(
      GOOGLE_WEB_PROPERTIES_TRIGGER, trial_name, group_name);
  if (new_id == EMPTY_ID && new_trigger_id == EMPTY_ID)
    return;

  base::AutoLock scoped_lock(lock_);
  if (new_id != EMPTY_ID)
    variation_ids_set_.insert(new_id);
  if (new_trigger_id != EMPTY_ID)
    variation_trigger_ids_set_.insert(new_trigger_id);

  UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& groups) {
  base::AutoLock scoped_lock(lock_);

  synthetic_variation_ids_set_.clear();
  for (const SyntheticTrialGroup& group : groups) {
    const VariationID id =
        GetGoogleVariationIDFromHashes(GOOGLE_WEB_PROPERTIES, group.id);
    if (id != EMPTY_ID)
      synthetic_variation_ids_set_.insert(id);
  }
  UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Register for additional cache updates. This is done first to avoid a race
  // that could cause registered FieldTrials to be missed.
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  base::FieldTrialList::AddObserver(this);

  base::TimeTicks before_time = base::TimeTicks::Now();

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);

  for (const auto& entry : initial_groups) {
    const VariationID id =
        GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, entry.trial_name,
                             entry.group_name);
    if (id != EMPTY_ID)
      variation_ids_set_.insert(id);

    const VariationID trigger_id =
        GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_TRIGGER, entry.trial_name,
                             entry.group_name);

    if (trigger_id != EMPTY_ID)
      variation_trigger_ids_set_.insert(trigger_id);
  }
  UpdateVariationIDsHeaderValue();

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Variations.HeaderConstructionTime",
      (base::TimeTicks::Now() - before_time).InMicroseconds(), 0,
      base::TimeDelta::FromSeconds(1).InMicroseconds(), 50);

  variation_ids_cache_initialized_ = true;
}

void VariationsHttpHeaderProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  // The header value is a serialized protobuffer of Variation IDs which is
  // base64 encoded before transmitting as a string.
  variation_ids_header_.clear();

  if (variation_ids_set_.empty() && default_variation_ids_set_.empty() &&
      variation_trigger_ids_set_.empty() && default_trigger_id_set_.empty() &&
      synthetic_variation_ids_set_.empty()) {
    return;
  }

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  const size_t total_id_count =
      variation_ids_set_.size() + variation_trigger_ids_set_.size();
  DCHECK_LE(total_id_count, 10U);
  UMA_HISTOGRAM_COUNTS_100("Variations.Headers.ExperimentCount",
                           total_id_count);
  if (total_id_count > 20)
    return;

  std::set<VariationID> all_variation_ids_set = GetAllVariationIds();
  std::set<VariationID> all_trigger_ids_set = default_trigger_id_set_;
  for (VariationID id : variation_trigger_ids_set_)
    all_trigger_ids_set.insert(id);

  ClientVariations proto;
  for (VariationID id : all_variation_ids_set)
    proto.add_variation_id(id);
  for (VariationID id : all_trigger_ids_set)
    proto.add_trigger_variation_id(id);

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  base::Base64Encode(serialized, &hashed);
  // If successful, swap the header value with the new one.
  // Note that the list of IDs and the header could be temporarily out of sync
  // if IDs are added as the header is recreated. The receiving servers are OK
  // with such discrepancies.
  variation_ids_header_ = hashed;
}

std::set<VariationID> VariationsHttpHeaderProvider::GetAllVariationIds() {
  lock_.AssertAcquired();

  std::set<VariationID> all_variation_ids_set = default_variation_ids_set_;
  for (VariationID id : variation_ids_set_)
    all_variation_ids_set.insert(id);
  for (VariationID id : synthetic_variation_ids_set_)
    all_variation_ids_set.insert(id);
  return all_variation_ids_set;
}

}  // namespace variations
