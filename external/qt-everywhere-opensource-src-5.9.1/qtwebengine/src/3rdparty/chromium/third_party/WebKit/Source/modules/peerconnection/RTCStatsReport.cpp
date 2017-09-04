// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCStatsReport.h"

namespace blink {

namespace {

template <typename T>
bool addPropertyValue(v8::Local<v8::Object>& v8Object,
                      v8::Isolate* isolate,
                      T name,
                      v8::Local<v8::Value> value) {
  return v8CallBoolean(v8Object->CreateDataProperty(
      isolate->GetCurrentContext(), v8String(isolate, name), value));
}

bool addPropertySequenceOfBooleans(v8::Local<v8::Object>& v8Object,
                                   v8::Isolate* isolate,
                                   WebString name,
                                   const WebVector<int>& webVector) {
  v8::Local<v8::Array> v8Array = v8::Array::New(isolate, webVector.size());
  for (size_t i = 0; i < webVector.size(); ++i) {
    if (!v8CallBoolean(v8Array->CreateDataProperty(
            isolate->GetCurrentContext(), static_cast<uint32_t>(i),
            v8::Boolean::New(isolate, static_cast<bool>(webVector[i])))))
      return false;
  }
  return addPropertyValue(v8Object, isolate, name, v8Array);
}

template <typename T>
bool addPropertySequenceOfNumbers(v8::Local<v8::Object>& v8Object,
                                  v8::Isolate* isolate,
                                  WebString name,
                                  const WebVector<T>& webVector) {
  v8::Local<v8::Array> v8Array = v8::Array::New(isolate, webVector.size());
  for (size_t i = 0; i < webVector.size(); ++i) {
    if (!v8CallBoolean(v8Array->CreateDataProperty(
            isolate->GetCurrentContext(), static_cast<uint32_t>(i),
            v8::Number::New(isolate, static_cast<double>(webVector[i])))))
      return false;
  }
  return addPropertyValue(v8Object, isolate, name, v8Array);
}

bool addPropertySequenceOfStrings(v8::Local<v8::Object>& v8Object,
                                  v8::Isolate* isolate,
                                  WebString name,
                                  const WebVector<WebString>& webVector) {
  v8::Local<v8::Array> v8Array = v8::Array::New(isolate, webVector.size());
  for (size_t i = 0; i < webVector.size(); ++i) {
    if (!v8CallBoolean(v8Array->CreateDataProperty(
            isolate->GetCurrentContext(), static_cast<uint32_t>(i),
            v8String(isolate, webVector[i]))))
      return false;
  }
  return addPropertyValue(v8Object, isolate, name, v8Array);
}

v8::Local<v8::Value> webRTCStatsToValue(ScriptState* scriptState,
                                        const WebRTCStats* stats) {
  v8::Isolate* isolate = scriptState->isolate();
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);

  bool success = true;
  success &=
      addPropertyValue(v8Object, isolate, "id", v8String(isolate, stats->id()));
  success &= addPropertyValue(v8Object, isolate, "timestamp",
                              v8::Number::New(isolate, stats->timestamp()));
  success &= addPropertyValue(v8Object, isolate, "type",
                              v8String(isolate, stats->type()));
  for (size_t i = 0; i < stats->membersCount() && success; ++i) {
    std::unique_ptr<WebRTCStatsMember> member = stats->getMember(i);
    if (!member->isDefined())
      continue;
    WebString name = member->name();
    switch (member->type()) {
      case WebRTCStatsMemberTypeBool:
        success &=
            addPropertyValue(v8Object, isolate, name,
                             v8::Boolean::New(isolate, member->valueBool()));
        break;
      case WebRTCStatsMemberTypeInt32:
        success &= addPropertyValue(
            v8Object, isolate, name,
            v8::Number::New(isolate,
                            static_cast<double>(member->valueInt32())));
        break;
      case WebRTCStatsMemberTypeUint32:
        success &= addPropertyValue(
            v8Object, isolate, name,
            v8::Number::New(isolate,
                            static_cast<double>(member->valueUint32())));
        break;
      case WebRTCStatsMemberTypeInt64:
        success &= addPropertyValue(
            v8Object, isolate, name,
            v8::Number::New(isolate,
                            static_cast<double>(member->valueInt64())));
        break;
      case WebRTCStatsMemberTypeUint64:
        success &= addPropertyValue(
            v8Object, isolate, name,
            v8::Number::New(isolate,
                            static_cast<double>(member->valueUint64())));
        break;
      case WebRTCStatsMemberTypeDouble:
        success &=
            addPropertyValue(v8Object, isolate, name,
                             v8::Number::New(isolate, member->valueDouble()));
        break;
      case WebRTCStatsMemberTypeString:
        success &= addPropertyValue(v8Object, isolate, name,
                                    v8String(isolate, member->valueString()));
        break;
      case WebRTCStatsMemberTypeSequenceBool:
        success &= addPropertySequenceOfBooleans(v8Object, isolate, name,
                                                 member->valueSequenceBool());
        break;
      case WebRTCStatsMemberTypeSequenceInt32:
        success &= addPropertySequenceOfNumbers(v8Object, isolate, name,
                                                member->valueSequenceInt32());
        break;
      case WebRTCStatsMemberTypeSequenceUint32:
        success &= addPropertySequenceOfNumbers(v8Object, isolate, name,
                                                member->valueSequenceUint32());
        break;
      case WebRTCStatsMemberTypeSequenceInt64:
        success &= addPropertySequenceOfNumbers(v8Object, isolate, name,
                                                member->valueSequenceInt64());
        break;
      case WebRTCStatsMemberTypeSequenceUint64:
        success &= addPropertySequenceOfNumbers(v8Object, isolate, name,
                                                member->valueSequenceUint64());
        break;
      case WebRTCStatsMemberTypeSequenceDouble:
        success &= addPropertySequenceOfNumbers(v8Object, isolate, name,
                                                member->valueSequenceDouble());
        break;
      case WebRTCStatsMemberTypeSequenceString:
        success &= addPropertySequenceOfStrings(v8Object, isolate, name,
                                                member->valueSequenceString());
        break;
      default:
        NOTREACHED();
    }
  }
  if (!success) {
    NOTREACHED();
    return v8::Undefined(isolate);
  }
  return v8Object;
}

class RTCStatsReportIterationSource final
    : public PairIterable<String, v8::Local<v8::Value>>::IterationSource {
 public:
  RTCStatsReportIterationSource(std::unique_ptr<WebRTCStatsReport> report)
      : m_report(std::move(report)) {}

  bool next(ScriptState* scriptState,
            String& key,
            v8::Local<v8::Value>& value,
            ExceptionState& exceptionState) override {
    std::unique_ptr<WebRTCStats> stats = m_report->next();
    if (!stats)
      return false;
    key = stats->id();
    value = webRTCStatsToValue(scriptState, stats.get());
    return true;
  }

 private:
  std::unique_ptr<WebRTCStatsReport> m_report;
};

}  // namespace

RTCStatsReport::RTCStatsReport(std::unique_ptr<WebRTCStatsReport> report)
    : m_report(std::move(report)) {}

PairIterable<String, v8::Local<v8::Value>>::IterationSource*
RTCStatsReport::startIteration(ScriptState*, ExceptionState&) {
  return new RTCStatsReportIterationSource(m_report->copyHandle());
}

bool RTCStatsReport::getMapEntry(ScriptState* scriptState,
                                 const String& key,
                                 v8::Local<v8::Value>& value,
                                 ExceptionState&) {
  std::unique_ptr<WebRTCStats> stats = m_report->getStats(key);
  if (!stats)
    return false;
  value = webRTCStatsToValue(scriptState, stats.get());
  return true;
}

}  // namespace blink
