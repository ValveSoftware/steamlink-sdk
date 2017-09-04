// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/browsing_data_counter.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"

namespace browsing_data {

BrowsingDataCounter::BrowsingDataCounter() {}

BrowsingDataCounter::~BrowsingDataCounter() {}

void BrowsingDataCounter::Init(PrefService* pref_service,
                               const Callback& callback) {
  DCHECK(!initialized_);
  callback_ = callback;
  pref_service_ = pref_service;
  pref_.Init(GetPrefName(), pref_service_,
             base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));
  period_.Init(
      browsing_data::prefs::kDeleteTimePeriod, pref_service_,
      base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));

  initialized_ = true;
  OnInitialized();
}

void BrowsingDataCounter::OnInitialized() {}

base::Time BrowsingDataCounter::GetPeriodStart() {
  return CalculateBeginDeleteTime(static_cast<TimePeriod>(*period_));
}

void BrowsingDataCounter::Restart() {
  DCHECK(initialized_);

  // If this data type was unchecked for deletion, we do not need to count it.
  if (!pref_service_->GetBoolean(GetPrefName()))
    return;

  callback_.Run(base::MakeUnique<Result>(this));

  Count();
}

void BrowsingDataCounter::ReportResult(ResultInt value) {
  DCHECK(initialized_);
  callback_.Run(base::MakeUnique<FinishedResult>(this, value));
}

void BrowsingDataCounter::ReportResult(std::unique_ptr<Result> result) {
  DCHECK(initialized_);
  callback_.Run(std::move(result));
}

PrefService* BrowsingDataCounter::GetPrefs() const {
  return pref_service_;
}

// BrowsingDataCounter::Result -------------------------------------------------

BrowsingDataCounter::Result::Result(const BrowsingDataCounter* source)
    : source_(source) {}

BrowsingDataCounter::Result::~Result() {}

bool BrowsingDataCounter::Result::Finished() const {
  return false;
}

// BrowsingDataCounter::FinishedResult -----------------------------------------

BrowsingDataCounter::FinishedResult::FinishedResult(
    const BrowsingDataCounter* source,
    ResultInt value)
    : Result(source), value_(value) {}

BrowsingDataCounter::FinishedResult::~FinishedResult() {}

bool BrowsingDataCounter::FinishedResult::Finished() const {
  return true;
}

BrowsingDataCounter::ResultInt BrowsingDataCounter::FinishedResult::Value()
    const {
  return value_;
}

}  // namespace browsing_data
