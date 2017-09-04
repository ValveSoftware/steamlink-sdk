// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCStatsReport_h
#define RTCStatsReport_h

#include "bindings/core/v8/Maplike.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebRTCStats.h"
#include "wtf/text/WTFString.h"

#include <map>

namespace blink {

// https://w3c.github.io/webrtc-pc/#rtcstatsreport-object
class RTCStatsReport final : public GarbageCollectedFinalized<RTCStatsReport>,
                             public ScriptWrappable,
                             public Maplike<String, v8::Local<v8::Value>> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCStatsReport(std::unique_ptr<WebRTCStatsReport>);

  // Maplike<String, v8::Local<v8::Value>>
  PairIterable<String, v8::Local<v8::Value>>::IterationSource* startIteration(
      ScriptState*,
      ExceptionState&) override;
  bool getMapEntry(ScriptState*,
                   const String& key,
                   v8::Local<v8::Value>&,
                   ExceptionState&) override;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 private:
  std::unique_ptr<WebRTCStatsReport> m_report;
};

}  // namespace blink

#endif  // RTCStatsReport_h
