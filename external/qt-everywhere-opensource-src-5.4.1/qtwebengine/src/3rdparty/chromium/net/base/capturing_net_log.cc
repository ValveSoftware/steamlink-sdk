// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

namespace net {

CapturingNetLog::CapturedEntry::CapturedEntry(
    EventType type,
    const base::TimeTicks& time,
    Source source,
    EventPhase phase,
    scoped_ptr<base::DictionaryValue> params)
    : type(type),
      time(time),
      source(source),
      phase(phase),
      params(params.Pass()) {
}

CapturingNetLog::CapturedEntry::CapturedEntry(const CapturedEntry& entry) {
  *this = entry;
}

CapturingNetLog::CapturedEntry::~CapturedEntry() {}

CapturingNetLog::CapturedEntry&
CapturingNetLog::CapturedEntry::operator=(const CapturedEntry& entry) {
  type = entry.type;
  time = entry.time;
  source = entry.source;
  phase = entry.phase;
  params.reset(entry.params ? entry.params->DeepCopy() : NULL);
  return *this;
}

bool CapturingNetLog::CapturedEntry::GetStringValue(
    const std::string& name,
    std::string* value) const {
  if (!params)
    return false;
  return params->GetString(name, value);
}

bool CapturingNetLog::CapturedEntry::GetIntegerValue(
    const std::string& name,
    int* value) const {
  if (!params)
    return false;
  return params->GetInteger(name, value);
}

bool CapturingNetLog::CapturedEntry::GetListValue(
    const std::string& name,
    base::ListValue** value) const {
  if (!params)
    return false;
  return params->GetList(name, value);
}

bool CapturingNetLog::CapturedEntry::GetNetErrorCode(int* value) const {
  return GetIntegerValue("net_error", value);
}

std::string CapturingNetLog::CapturedEntry::GetParamsJson() const {
  if (!params)
    return std::string();
  std::string json;
  base::JSONWriter::Write(params.get(), &json);
  return json;
}

CapturingNetLog::Observer::Observer() {}

CapturingNetLog::Observer::~Observer() {}

void CapturingNetLog::Observer::GetEntries(
    CapturedEntryList* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = captured_entries_;
}

void CapturingNetLog::Observer::GetEntriesForSource(
    NetLog::Source source,
    CapturedEntryList* entry_list) const {
  base::AutoLock lock(lock_);
  entry_list->clear();
  for (CapturedEntryList::const_iterator entry = captured_entries_.begin();
       entry != captured_entries_.end(); ++entry) {
    if (entry->source.id == source.id)
      entry_list->push_back(*entry);
  }
}

size_t CapturingNetLog::Observer::GetSize() const {
  base::AutoLock lock(lock_);
  return captured_entries_.size();
}

void CapturingNetLog::Observer::Clear() {
  base::AutoLock lock(lock_);
  captured_entries_.clear();
}

void CapturingNetLog::Observer::OnAddEntry(const net::NetLog::Entry& entry) {
  // Only BoundNetLogs without a NetLog should have an invalid source.
  CHECK(entry.source().IsValid());

  // Using Dictionaries instead of Values makes checking values a little
  // simpler.
  base::DictionaryValue* param_dict = NULL;
  base::Value* param_value = entry.ParametersToValue();
  if (param_value && !param_value->GetAsDictionary(&param_dict))
    delete param_value;

  // Only need to acquire the lock when accessing class variables.
  base::AutoLock lock(lock_);
  captured_entries_.push_back(
      CapturedEntry(entry.type(),
                    base::TimeTicks::Now(),
                    entry.source(),
                    entry.phase(),
                    scoped_ptr<base::DictionaryValue>(param_dict)));
}

CapturingNetLog::CapturingNetLog() {
  AddThreadSafeObserver(&capturing_net_log_observer_, LOG_ALL_BUT_BYTES);
}

CapturingNetLog::~CapturingNetLog() {
  RemoveThreadSafeObserver(&capturing_net_log_observer_);
}

void CapturingNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  SetObserverLogLevel(&capturing_net_log_observer_, log_level);
}

void CapturingNetLog::GetEntries(
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntries(entry_list);
}

void CapturingNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntriesForSource(source, entry_list);
}

size_t CapturingNetLog::GetSize() const {
  return capturing_net_log_observer_.GetSize();
}

void CapturingNetLog::Clear() {
  capturing_net_log_observer_.Clear();
}

CapturingBoundNetLog::CapturingBoundNetLog()
    : net_log_(BoundNetLog::Make(&capturing_net_log_,
                                 net::NetLog::SOURCE_NONE)) {
}

CapturingBoundNetLog::~CapturingBoundNetLog() {}

void CapturingBoundNetLog::GetEntries(
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntries(entry_list);
}

void CapturingBoundNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntriesForSource(source, entry_list);
}

size_t CapturingBoundNetLog::GetSize() const {
  return capturing_net_log_.GetSize();
}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_.Clear();
}

void CapturingBoundNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  capturing_net_log_.SetLogLevel(log_level);
}

}  // namespace net
