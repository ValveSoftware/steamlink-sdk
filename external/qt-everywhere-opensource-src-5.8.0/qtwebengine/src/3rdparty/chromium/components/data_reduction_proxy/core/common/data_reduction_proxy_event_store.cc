// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"

namespace {

const size_t kMaxEventsToStore = 100;

// Used by Data Reduction Proxy feedback reports. If the last bypass happened in
// the last 5 minutes, the url will be cropped to only the host.
const int kLastBypassTimeDeltaInMinutes = 5;

struct StringToConstant {
  const char* name;
  const int constant;
};

// Creates an associative array of the enum name to enum value for
// DataReductionProxyBypassType. Ensures that the same enum space is used
// without having to keep an enum map in sync.
const StringToConstant kDataReductionProxyBypassEventTypeTable[] = {
#define BYPASS_EVENT_TYPE(label, value) { \
    # label, data_reduction_proxy::BYPASS_EVENT_TYPE_ ## label },
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_type_list.h"
#undef BYPASS_EVENT_TYPE
};

// Creates an associate array of the enum name to enum value for
// DataReductionProxyBypassAction. Ensures that the same enum space is used
// without having to keep an enum map in sync.
const StringToConstant kDataReductionProxyBypassActionTypeTable[] = {
#define BYPASS_ACTION_TYPE(label, value)                       \
  { #label, data_reduction_proxy::BYPASS_ACTION_TYPE_##label } \
  ,
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_action_list.h"
#undef BYPASS_ACTION_TYPE
};

std::string JoinListValueStrings(base::ListValue* list_value) {
  std::vector<std::string> values;
  for (const auto& value : *list_value) {
    std::string value_string;
    if (!value->GetAsString(&value_string))
      return std::string();

    values.push_back(value_string);
  }

  return base::JoinString(values, ";");
}

}  // namespace

namespace data_reduction_proxy {

// static
void DataReductionProxyEventStore::AddConstants(
    base::DictionaryValue* constants_dict) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  for (size_t i = 0;
       i < arraysize(kDataReductionProxyBypassEventTypeTable); ++i) {
    dict->SetInteger(kDataReductionProxyBypassEventTypeTable[i].name,
                     kDataReductionProxyBypassEventTypeTable[i].constant);
  }

  constants_dict->Set("dataReductionProxyBypassEventType", std::move(dict));

  dict.reset(new base::DictionaryValue());
  for (size_t i = 0; i < arraysize(kDataReductionProxyBypassActionTypeTable);
       ++i) {
    dict->SetInteger(kDataReductionProxyBypassActionTypeTable[i].name,
                     kDataReductionProxyBypassActionTypeTable[i].constant);
  }

  constants_dict->Set("dataReductionProxyBypassActionType", std::move(dict));
}

DataReductionProxyEventStore::DataReductionProxyEventStore()
    : enabled_(false),
      secure_proxy_check_state_(CHECK_UNKNOWN),
      expiration_ticks_(0) {
}

DataReductionProxyEventStore::~DataReductionProxyEventStore() {
  STLDeleteElements(&stored_events_);
}

base::Value* DataReductionProxyEventStore::GetSummaryValue() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::DictionaryValue> data_reduction_proxy_values(
      new base::DictionaryValue());
  data_reduction_proxy_values->SetBoolean("enabled", enabled_);

  base::Value* current_configuration = current_configuration_.get();
  if (current_configuration != nullptr) {
    data_reduction_proxy_values->Set("proxy_config",
                                     current_configuration->DeepCopy());
  }

  switch (secure_proxy_check_state_) {
    case CHECK_PENDING:
      data_reduction_proxy_values->SetString("probe", "Pending");
      break;
    case CHECK_SUCCESS:
      data_reduction_proxy_values->SetString("probe", "Success");
      break;
    case CHECK_FAILED:
      data_reduction_proxy_values->SetString("probe", "Failed");
      break;
    case CHECK_UNKNOWN:
      break;
    default:
      NOTREACHED();
      break;
  }

  base::Value* last_bypass_event = last_bypass_event_.get();
  if (last_bypass_event != nullptr) {
    int current_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
    if (expiration_ticks_ > current_time_ticks_ms) {
      data_reduction_proxy_values->Set("last_bypass",
                                       last_bypass_event->DeepCopy());
    }
  }

  base::ListValue* eventsList = new base::ListValue();
  for (size_t i = 0; i < stored_events_.size(); ++i)
    eventsList->Append(stored_events_[i]->DeepCopy());

  data_reduction_proxy_values->Set("events", eventsList);

  return data_reduction_proxy_values.release();
}

void DataReductionProxyEventStore::AddEvent(
    std::unique_ptr<base::Value> event) {
  if (stored_events_.size() == kMaxEventsToStore) {
    base::Value* head = stored_events_.front();
    stored_events_.pop_front();
    delete head;
  }

  stored_events_.push_back(event.release());
}

void DataReductionProxyEventStore::AddEnabledEvent(
    std::unique_ptr<base::Value> event,
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
  if (enabled)
    current_configuration_.reset(event->DeepCopy());
  else
    current_configuration_.reset();
  AddEvent(std::move(event));
}

void DataReductionProxyEventStore::AddEventAndSecureProxyCheckState(
    std::unique_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  secure_proxy_check_state_ = state;
  AddEvent(std::move(event));
}

void DataReductionProxyEventStore::AddAndSetLastBypassEvent(
    std::unique_ptr<base::Value> event,
    int64_t expiration_ticks) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_bypass_event_.reset(event->DeepCopy());
  expiration_ticks_ = expiration_ticks;
  AddEvent(std::move(event));
}

std::string DataReductionProxyEventStore::GetHttpProxyList() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !current_configuration_)
    return std::string();

  base::DictionaryValue* config_dict;
  if (!current_configuration_->GetAsDictionary(&config_dict))
    return std::string();

  base::DictionaryValue* params_dict;
  if (!config_dict->GetDictionary("params", &params_dict))
    return std::string();

  base::ListValue* proxy_list;
  if (!params_dict->GetList("http_proxy_list", &proxy_list))
    return std::string();

  return JoinListValueStrings(proxy_list);
}

std::string DataReductionProxyEventStore::SanitizedLastBypassEvent() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !last_bypass_event_)
    return std::string();

  base::DictionaryValue* bypass_dict;
  base::DictionaryValue* params_dict;
  if (!last_bypass_event_->GetAsDictionary(&bypass_dict))
    return std::string();

  if (!bypass_dict->GetDictionary("params", &params_dict))
    return std::string();

  // Explicitly add parameters to prevent automatic adding of new parameters.
  std::unique_ptr<base::DictionaryValue> last_bypass(
      new base::DictionaryValue());

  std::string str_value;
  int int_value;
  if (params_dict->GetInteger("bypass_type", &int_value))
    last_bypass->SetInteger("bypass_type", int_value);

  if (params_dict->GetInteger("bypass_action_type", &int_value))
    last_bypass->SetInteger("bypass_action", int_value);

  if (params_dict->GetString("bypass_duration_seconds", &str_value))
    last_bypass->SetString("bypass_seconds", str_value);

  bool truncate_url_to_host = true;
  if (bypass_dict->GetString("time", &str_value)) {
    last_bypass->SetString("bypass_time", str_value);

    int64_t bypass_ticks_ms;
    base::StringToInt64(str_value, &bypass_ticks_ms);

    base::TimeTicks bypass_ticks =
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(bypass_ticks_ms);

    // If the last bypass happened in the last 5 minutes, don't crop the url to
    // the host.
    if (base::TimeTicks::Now() - bypass_ticks <
        base::TimeDelta::FromMinutes(kLastBypassTimeDeltaInMinutes)) {
      truncate_url_to_host = false;
    }
  }

  if (params_dict->GetString("method", &str_value))
    last_bypass->SetString("method", str_value);

  if (params_dict->GetString("url", &str_value)) {
    GURL url(str_value);
    if (truncate_url_to_host) {
      last_bypass->SetString("url", url.host());
    } else {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      GURL clean_url = url.ReplaceComponents(replacements);
      last_bypass->SetString("url", clean_url.spec());
    }
  }

  std::string json;
  base::JSONWriter::Write(*last_bypass.get(), &json);
  return json;
}

}  // namespace data_reduction_proxy
