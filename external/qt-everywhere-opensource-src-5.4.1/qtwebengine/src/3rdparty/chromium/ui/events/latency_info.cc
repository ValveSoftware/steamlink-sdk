// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "ui/events/latency_info.h"

#include <algorithm>

namespace {

const size_t kMaxLatencyInfoNumber = 100;

const char* GetComponentName(ui::LatencyComponentType type) {
#define CASE_TYPE(t) case ui::t:  return #t
  switch (type) {
    CASE_TYPE(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_BEGIN_PLUGIN_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_UI_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_ACKED_TOUCH_COMPONENT);
    CASE_TYPE(WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT);
    CASE_TYPE(LATENCY_INFO_LIST_TERMINATED_OVERFLOW_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_TERMINATED_PLUGIN_COMPONENT);
    default:
      DLOG(WARNING) << "Unhandled LatencyComponentType.\n";
      break;
  }
#undef CASE_TYPE
  return "unknown";
}

bool IsTerminalComponent(ui::LatencyComponentType type) {
  switch (type) {
    case ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT:
    case ui::LATENCY_INFO_LIST_TERMINATED_OVERFLOW_COMPONENT:
    case ui::INPUT_EVENT_LATENCY_TERMINATED_PLUGIN_COMPONENT:
      return true;
    default:
      return false;
  }
}

bool IsBeginComponent(ui::LatencyComponentType type) {
  return (type == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT ||
          type == ui::INPUT_EVENT_LATENCY_BEGIN_PLUGIN_COMPONENT);
}

// This class is for converting latency info to trace buffer friendly format.
class LatencyInfoTracedValue : public base::debug::ConvertableToTraceFormat {
 public:
  static scoped_refptr<ConvertableToTraceFormat> FromValue(
      scoped_ptr<base::Value> value);

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE;

 private:
  explicit LatencyInfoTracedValue(base::Value* value);
  virtual ~LatencyInfoTracedValue();

  scoped_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(LatencyInfoTracedValue);
};

scoped_refptr<base::debug::ConvertableToTraceFormat>
LatencyInfoTracedValue::FromValue(scoped_ptr<base::Value> value) {
  return scoped_refptr<base::debug::ConvertableToTraceFormat>(
      new LatencyInfoTracedValue(value.release()));
}

LatencyInfoTracedValue::~LatencyInfoTracedValue() {
}

void LatencyInfoTracedValue::AppendAsTraceFormat(std::string* out) const {
  std::string tmp;
  base::JSONWriter::Write(value_.get(), &tmp);
  *out += tmp;
}

LatencyInfoTracedValue::LatencyInfoTracedValue(base::Value* value)
    : value_(value) {
}

// Converts latencyinfo into format that can be dumped into trace buffer.
scoped_refptr<base::debug::ConvertableToTraceFormat> AsTraceableData(
    const ui::LatencyInfo& latency) {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  for (ui::LatencyInfo::LatencyMap::const_iterator it =
           latency.latency_components.begin();
       it != latency.latency_components.end(); ++it) {
    base::DictionaryValue* component_info = new base::DictionaryValue();
    component_info->SetDouble("comp_id", it->first.second);
    component_info->SetDouble("time", it->second.event_time.ToInternalValue());
    component_info->SetDouble("count", it->second.event_count);
    record_data->Set(GetComponentName(it->first.first), component_info);
  }
  record_data->SetDouble("trace_id", latency.trace_id);
  return LatencyInfoTracedValue::FromValue(record_data.PassAs<base::Value>());
}

}  // namespace

namespace ui {

LatencyInfo::LatencyInfo() : trace_id(-1), terminated(false) {
}

LatencyInfo::~LatencyInfo() {
}

bool LatencyInfo::Verify(const std::vector<LatencyInfo>& latency_info,
                         const char* referring_msg) {
  if (latency_info.size() > kMaxLatencyInfoNumber) {
    LOG(ERROR) << referring_msg << ", LatencyInfo vector size "
               << latency_info.size() << " is too big.";
    return false;
  }
  return true;
}

void LatencyInfo::CopyLatencyFrom(const LatencyInfo& other,
                                  LatencyComponentType type) {
  for (LatencyMap::const_iterator it = other.latency_components.begin();
       it != other.latency_components.end();
       ++it) {
    if (it->first.first == type) {
      AddLatencyNumberWithTimestamp(it->first.first,
                                    it->first.second,
                                    it->second.sequence_number,
                                    it->second.event_time,
                                    it->second.event_count);
    }
  }
}

void LatencyInfo::AddNewLatencyFrom(const LatencyInfo& other) {
    for (LatencyMap::const_iterator it = other.latency_components.begin();
         it != other.latency_components.end();
         ++it) {
      if (!FindLatency(it->first.first, it->first.second, NULL)) {
        AddLatencyNumberWithTimestamp(it->first.first,
                                      it->first.second,
                                      it->second.sequence_number,
                                      it->second.event_time,
                                      it->second.event_count);
      }
    }
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component,
                                   int64 id,
                                   int64 component_sequence_number) {
  AddLatencyNumberWithTimestamp(component, id, component_sequence_number,
                                base::TimeTicks::HighResNow(), 1);
}

void LatencyInfo::AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                                int64 id,
                                                int64 component_sequence_number,
                                                base::TimeTicks time,
                                                uint32 event_count) {
  if (IsBeginComponent(component)) {
    // Should only ever add begin component once.
    CHECK_EQ(-1, trace_id);
    trace_id = component_sequence_number;
    TRACE_EVENT_ASYNC_BEGIN0("benchmark",
                             "InputLatency",
                             TRACE_ID_DONT_MANGLE(trace_id));
    TRACE_EVENT_FLOW_BEGIN0(
        "input", "LatencyInfo.Flow", TRACE_ID_DONT_MANGLE(trace_id));
  }

  LatencyMap::key_type key = std::make_pair(component, id);
  LatencyMap::iterator it = latency_components.find(key);
  if (it == latency_components.end()) {
    LatencyComponent info = {component_sequence_number, time, event_count};
    latency_components[key] = info;
  } else {
    it->second.sequence_number = std::max(component_sequence_number,
                                          it->second.sequence_number);
    uint32 new_count = event_count + it->second.event_count;
    if (event_count > 0 && new_count != 0) {
      // Do a weighted average, so that the new event_time is the average of
      // the times of events currently in this structure with the time passed
      // into this method.
      it->second.event_time += (time - it->second.event_time) * event_count /
          new_count;
      it->second.event_count = new_count;
    }
  }

  if (IsTerminalComponent(component) && trace_id != -1) {
    // Should only ever add terminal component once.
    CHECK(!terminated);
    terminated = true;
    TRACE_EVENT_ASYNC_END1("benchmark",
                           "InputLatency",
                           TRACE_ID_DONT_MANGLE(trace_id),
                           "data", AsTraceableData(*this));
    TRACE_EVENT_FLOW_END0(
        "input", "LatencyInfo.Flow", TRACE_ID_DONT_MANGLE(trace_id));
  }
}

bool LatencyInfo::FindLatency(LatencyComponentType type,
                              int64 id,
                              LatencyComponent* output) const {
  LatencyMap::const_iterator it = latency_components.find(
      std::make_pair(type, id));
  if (it == latency_components.end())
    return false;
  if (output)
    *output = it->second;
  return true;
}

void LatencyInfo::RemoveLatency(LatencyComponentType type) {
  LatencyMap::iterator it = latency_components.begin();
  while (it != latency_components.end()) {
    if (it->first.first == type) {
      LatencyMap::iterator tmp = it;
      ++it;
      latency_components.erase(tmp);
    } else {
      it++;
    }
  }
}

void LatencyInfo::Clear() {
  latency_components.clear();
}

void LatencyInfo::TraceEventType(const char* event_type) {
  TRACE_EVENT_ASYNC_STEP_INTO0("benchmark",
                               "InputLatency",
                               TRACE_ID_DONT_MANGLE(trace_id),
                               event_type);
}

}  // namespace ui
