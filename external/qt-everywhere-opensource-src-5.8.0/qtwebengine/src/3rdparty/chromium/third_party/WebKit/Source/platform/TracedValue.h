// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "base/memory/ref_counted.h"
#include "platform/EventTracer.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {

// TracedValue copies all passed names and values and doesn't retain references.
class PLATFORM_EXPORT TracedValue final {
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

    // This will be moved (and become null) when TracedValue is passed to
    // EventTracer::addTraceEvent().
    std::unique_ptr<base::trace_event::TracedValue> m_tracedValue;

    friend class EventTracer;
};

} // namespace blink

#endif // TracedValue_h
