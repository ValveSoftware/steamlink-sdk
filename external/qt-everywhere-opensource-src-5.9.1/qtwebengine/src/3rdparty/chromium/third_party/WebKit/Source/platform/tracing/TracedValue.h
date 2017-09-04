// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "base/trace_event/trace_event_argument.h"
#include "platform/PlatformExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

// TracedValue copies all passed names and values and doesn't retain references.
class PLATFORM_EXPORT TracedValue final
    : public base::trace_event::ConvertableToTraceFormat {
  WTF_MAKE_NONCOPYABLE(TracedValue);

 public:
  ~TracedValue();

  static std::unique_ptr<TracedValue> create();

  void endDictionary();
  void endArray();

  void setInteger(const char* name, int value);
  void setDouble(const char* name, double);
  void setBoolean(const char* name, bool value);
  void setString(const char* name, const String& value);
  void beginArray(const char* name);
  void beginDictionary(const char* name);

  void pushInteger(int);
  void pushDouble(double);
  void pushBoolean(bool);
  void pushString(const String&);
  void beginArray();
  void beginDictionary();

  String toString() const;

 private:
  TracedValue();

  // ConvertableToTraceFormat

  // Hide this method from blink code.
  using base::trace_event::ConvertableToTraceFormat::ToString;
  void AppendAsTraceFormat(std::string*) const final;
  void EstimateTraceMemoryOverhead(
      base::trace_event::TraceEventMemoryOverhead*) final;

  base::trace_event::TracedValue m_tracedValue;
};

}  // namespace blink

#endif  // TracedValue_h
