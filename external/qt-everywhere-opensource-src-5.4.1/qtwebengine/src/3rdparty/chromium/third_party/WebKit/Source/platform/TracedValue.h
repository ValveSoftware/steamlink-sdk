// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "platform/EventTracer.h"

#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class JSONValue;

class PLATFORM_EXPORT TracedValue : public TraceEvent::ConvertableToTraceFormat {
    WTF_MAKE_NONCOPYABLE(TracedValue);
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> fromJSONValue(PassRefPtr<JSONValue> value)
    {
        return adoptRef(new TracedValue(value));
    }

    String asTraceFormat() const OVERRIDE;

private:
    explicit TracedValue(PassRefPtr<JSONValue>);
    virtual ~TracedValue();

    RefPtr<JSONValue> m_value;
};

} // namespace WebCore

#endif // TracedValue_h
