// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Returns parameters for logging data transferred events. Includes number of
// bytes transferred and, if the log level indicates bytes should be logged and
// |byte_count| > 0, the bytes themselves.  The bytes are hex-encoded, since
// base::StringValue only supports UTF-8.
base::Value* BytesTransferredCallback(int byte_count,
                                      const char* bytes,
                                      NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("byte_count", byte_count);
  if (NetLog::IsLoggingBytes(log_level) && byte_count > 0)
    dict->SetString("hex_encoded_bytes", base::HexEncode(bytes, byte_count));
  return dict;
}

base::Value* SourceEventParametersCallback(const NetLog::Source source,
                                           NetLog::LogLevel /* log_level */) {
  if (!source.IsValid())
    return NULL;
  base::DictionaryValue* event_params = new base::DictionaryValue();
  source.AddToEventParameters(event_params);
  return event_params;
}

base::Value* NetLogIntegerCallback(const char* name,
                                   int value,
                                   NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* event_params = new base::DictionaryValue();
  event_params->SetInteger(name, value);
  return event_params;
}

base::Value* NetLogInt64Callback(const char* name,
                                 int64 value,
                                 NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* event_params = new base::DictionaryValue();
  event_params->SetString(name, base::Int64ToString(value));
  return event_params;
}

base::Value* NetLogStringCallback(const char* name,
                                  const std::string* value,
                                  NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* event_params = new base::DictionaryValue();
  event_params->SetString(name, *value);
  return event_params;
}

base::Value* NetLogString16Callback(const char* name,
                                    const base::string16* value,
                                    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* event_params = new base::DictionaryValue();
  event_params->SetString(name, *value);
  return event_params;
}

}  // namespace

// LoadTimingInfo requires this be 0.
const uint32 NetLog::Source::kInvalidId = 0;

NetLog::Source::Source() : type(SOURCE_NONE), id(kInvalidId) {
}

NetLog::Source::Source(SourceType type, uint32 id) : type(type), id(id) {
}

bool NetLog::Source::IsValid() const {
  return id != kInvalidId;
}

void NetLog::Source::AddToEventParameters(
    base::DictionaryValue* event_params) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("type", static_cast<int>(type));
  dict->SetInteger("id", static_cast<int>(id));
  event_params->Set("source_dependency", dict);
}

NetLog::ParametersCallback NetLog::Source::ToEventParametersCallback() const {
  return base::Bind(&SourceEventParametersCallback, *this);
}

// static
bool NetLog::Source::FromEventParameters(base::Value* event_params,
                                         Source* source) {
  base::DictionaryValue* dict;
  base::DictionaryValue* source_dict;
  int source_id;
  int source_type;
  if (!event_params ||
      !event_params->GetAsDictionary(&dict) ||
      !dict->GetDictionary("source_dependency", &source_dict) ||
      !source_dict->GetInteger("id", &source_id) ||
      !source_dict->GetInteger("type", &source_type)) {
    *source = Source();
    return false;
  }

  DCHECK_LE(0, source_id);
  DCHECK_LT(source_type, NetLog::SOURCE_COUNT);
  *source = Source(static_cast<SourceType>(source_type), source_id);
  return true;
}

base::Value* NetLog::Entry::ToValue() const {
  base::DictionaryValue* entry_dict(new base::DictionaryValue());

  entry_dict->SetString("time", TickCountToString(data_->time));

  // Set the entry source.
  base::DictionaryValue* source_dict = new base::DictionaryValue();
  source_dict->SetInteger("id", data_->source.id);
  source_dict->SetInteger("type", static_cast<int>(data_->source.type));
  entry_dict->Set("source", source_dict);

  // Set the event info.
  entry_dict->SetInteger("type", static_cast<int>(data_->type));
  entry_dict->SetInteger("phase", static_cast<int>(data_->phase));

  // Set the event-specific parameters.
  if (data_->parameters_callback) {
    base::Value* value = data_->parameters_callback->Run(log_level_);
    if (value)
      entry_dict->Set("params", value);
  }

  return entry_dict;
}

base::Value* NetLog::Entry::ParametersToValue() const {
  if (data_->parameters_callback)
    return data_->parameters_callback->Run(log_level_);
  return NULL;
}

NetLog::EntryData::EntryData(
    EventType type,
    Source source,
    EventPhase phase,
    base::TimeTicks time,
    const ParametersCallback* parameters_callback)
    : type(type),
      source(source),
      phase(phase),
      time(time),
      parameters_callback(parameters_callback) {
}

NetLog::EntryData::~EntryData() {
}

NetLog::Entry::Entry(const EntryData* data, LogLevel log_level)
    : data_(data), log_level_(log_level) {
}

NetLog::Entry::~Entry() {
}

NetLog::ThreadSafeObserver::ThreadSafeObserver() : log_level_(LOG_NONE),
                                                   net_log_(NULL) {
}

NetLog::ThreadSafeObserver::~ThreadSafeObserver() {
  // Make sure we aren't watching a NetLog on destruction.  Because the NetLog
  // may pass events to each observer on multiple threads, we cannot safely
  // stop watching a NetLog automatically from a parent class.
  DCHECK(!net_log_);
}

NetLog::LogLevel NetLog::ThreadSafeObserver::log_level() const {
  DCHECK(net_log_);
  return log_level_;
}

NetLog* NetLog::ThreadSafeObserver::net_log() const {
  return net_log_;
}

void NetLog::ThreadSafeObserver::OnAddEntryData(const EntryData& entry_data) {
  OnAddEntry(Entry(&entry_data, log_level()));
}

NetLog::NetLog()
    : last_id_(0),
      base_log_level_(LOG_NONE),
      effective_log_level_(LOG_NONE) {
}

NetLog::~NetLog() {
}

void NetLog::AddGlobalEntry(EventType type) {
  AddEntry(type,
           Source(net::NetLog::SOURCE_NONE, NextID()),
           net::NetLog::PHASE_NONE,
           NULL);
}

void NetLog::AddGlobalEntry(
    EventType type,
    const NetLog::ParametersCallback& parameters_callback) {
  AddEntry(type,
           Source(net::NetLog::SOURCE_NONE, NextID()),
           net::NetLog::PHASE_NONE,
           &parameters_callback);
}

uint32 NetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

void NetLog::SetBaseLogLevel(LogLevel log_level) {
  base::AutoLock lock(lock_);
  base_log_level_ = log_level;

  UpdateLogLevel();
}

NetLog::LogLevel NetLog::GetLogLevel() const {
  base::subtle::Atomic32 log_level =
      base::subtle::NoBarrier_Load(&effective_log_level_);
  return static_cast<net::NetLog::LogLevel>(log_level);
}

void NetLog::AddThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer,
    LogLevel log_level) {
  DCHECK_NE(LOG_NONE, log_level);
  base::AutoLock lock(lock_);

  DCHECK(!observer->net_log_);
  DCHECK_EQ(LOG_NONE, observer->log_level_);
  observers_.AddObserver(observer);
  observer->net_log_ = this;
  observer->log_level_ = log_level;
  UpdateLogLevel();
}

void NetLog::SetObserverLogLevel(
    net::NetLog::ThreadSafeObserver* observer,
    LogLevel log_level) {
  DCHECK_NE(LOG_NONE, log_level);
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  DCHECK_EQ(this, observer->net_log_);
  DCHECK_NE(LOG_NONE, observer->log_level_);
  observer->log_level_ = log_level;
  UpdateLogLevel();
}

void NetLog::RemoveThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  DCHECK_EQ(this, observer->net_log_);
  DCHECK_NE(LOG_NONE, observer->log_level_);
  observers_.RemoveObserver(observer);
  observer->net_log_ = NULL;
  observer->log_level_ = LOG_NONE;
  UpdateLogLevel();
}

void NetLog::UpdateLogLevel() {
  lock_.AssertAcquired();

  // Look through all the observers and find the finest granularity
  // log level (higher values of the enum imply *lower* log levels).
  LogLevel new_effective_log_level = base_log_level_;
  ObserverListBase<ThreadSafeObserver>::Iterator it(observers_);
  ThreadSafeObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    new_effective_log_level =
        std::min(new_effective_log_level, observer->log_level());
  }
  base::subtle::NoBarrier_Store(&effective_log_level_,
                                new_effective_log_level);
}

// static
std::string NetLog::TickCountToString(const base::TimeTicks& time) {
  int64 delta_time = (time - base::TimeTicks()).InMilliseconds();
  return base::Int64ToString(delta_time);
}

// static
const char* NetLog::EventTypeToString(EventType event) {
  switch (event) {
#define EVENT_TYPE(label) case TYPE_ ## label: return #label;
#include "net/base/net_log_event_type_list.h"
#undef EVENT_TYPE
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
base::Value* NetLog::GetEventTypesAsValue() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  for (int i = 0; i < EVENT_COUNT; ++i) {
    dict->SetInteger(EventTypeToString(static_cast<EventType>(i)), i);
  }
  return dict;
}

// static
const char* NetLog::SourceTypeToString(SourceType source) {
  switch (source) {
#define SOURCE_TYPE(label) case SOURCE_ ## label: return #label;
#include "net/base/net_log_source_type_list.h"
#undef SOURCE_TYPE
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
base::Value* NetLog::GetSourceTypesAsValue() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  for (int i = 0; i < SOURCE_COUNT; ++i) {
    dict->SetInteger(SourceTypeToString(static_cast<SourceType>(i)), i);
  }
  return dict;
}

// static
const char* NetLog::EventPhaseToString(EventPhase phase) {
  switch (phase) {
    case PHASE_BEGIN:
      return "PHASE_BEGIN";
    case PHASE_END:
      return "PHASE_END";
    case PHASE_NONE:
      return "PHASE_NONE";
  }
  NOTREACHED();
  return NULL;
}

// static
bool NetLog::IsLoggingBytes(LogLevel log_level) {
  return log_level == NetLog::LOG_ALL;
}

// static
bool NetLog::IsLogging(LogLevel log_level) {
  return log_level < NetLog::LOG_NONE;
}

// static
NetLog::ParametersCallback NetLog::IntegerCallback(const char* name,
                                                   int value) {
  return base::Bind(&NetLogIntegerCallback, name, value);
}

// static
NetLog::ParametersCallback NetLog::Int64Callback(const char* name,
                                                 int64 value) {
  return base::Bind(&NetLogInt64Callback, name, value);
}

// static
NetLog::ParametersCallback NetLog::StringCallback(const char* name,
                                                  const std::string* value) {
  DCHECK(value);
  return base::Bind(&NetLogStringCallback, name, value);
}

// static
NetLog::ParametersCallback NetLog::StringCallback(const char* name,
                                                  const base::string16* value) {
  DCHECK(value);
  return base::Bind(&NetLogString16Callback, name, value);
}

void NetLog::AddEntry(EventType type,
                      const Source& source,
                      EventPhase phase,
                      const NetLog::ParametersCallback* parameters_callback) {
  if (GetLogLevel() == LOG_NONE)
    return;
  EntryData entry_data(type, source, phase, base::TimeTicks::Now(),
                       parameters_callback);

  // Notify all of the log observers.
  base::AutoLock lock(lock_);
  FOR_EACH_OBSERVER(ThreadSafeObserver, observers_, OnAddEntryData(entry_data));
}

void BoundNetLog::AddEntry(NetLog::EventType type,
                           NetLog::EventPhase phase) const {
  if (!net_log_)
    return;
  net_log_->AddEntry(type, source_, phase, NULL);
}

void BoundNetLog::AddEntry(
    NetLog::EventType type,
    NetLog::EventPhase phase,
    const NetLog::ParametersCallback& get_parameters) const {
  if (!net_log_)
    return;
  net_log_->AddEntry(type, source_, phase, &get_parameters);
}

void BoundNetLog::AddEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_NONE);
}

void BoundNetLog::AddEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_NONE, get_parameters);
}

void BoundNetLog::BeginEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_BEGIN);
}

void BoundNetLog::BeginEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_BEGIN, get_parameters);
}

void BoundNetLog::EndEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_END);
}

void BoundNetLog::EndEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_END, get_parameters);
}

void BoundNetLog::AddEventWithNetErrorCode(NetLog::EventType event_type,
                                           int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    AddEvent(event_type);
  } else {
    AddEvent(event_type, NetLog::IntegerCallback("net_error", net_error));
  }
}

void BoundNetLog::EndEventWithNetErrorCode(NetLog::EventType event_type,
                                           int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    EndEvent(event_type);
  } else {
    EndEvent(event_type, NetLog::IntegerCallback("net_error", net_error));
  }
}

void BoundNetLog::AddByteTransferEvent(NetLog::EventType event_type,
                                       int byte_count,
                                       const char* bytes) const {
  AddEvent(event_type, base::Bind(BytesTransferredCallback, byte_count, bytes));
}

NetLog::LogLevel BoundNetLog::GetLogLevel() const {
  if (net_log_)
    return net_log_->GetLogLevel();
  return NetLog::LOG_NONE;
}

bool BoundNetLog::IsLoggingBytes() const {
  return NetLog::IsLoggingBytes(GetLogLevel());
}

bool BoundNetLog::IsLogging() const {
  return NetLog::IsLogging(GetLogLevel());
}

// static
BoundNetLog BoundNetLog::Make(NetLog* net_log,
                              NetLog::SourceType source_type) {
  if (!net_log)
    return BoundNetLog();

  NetLog::Source source(source_type, net_log->NextID());
  return BoundNetLog(source, net_log);
}

}  // namespace net
