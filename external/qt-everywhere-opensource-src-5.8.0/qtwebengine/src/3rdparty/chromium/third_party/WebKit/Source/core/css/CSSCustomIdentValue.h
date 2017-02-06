// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCustomIdentValue_h
#define CSSCustomIdentValue_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"

namespace blink {

class CSSCustomIdentValue : public CSSValue {
public:
    static CSSCustomIdentValue* create(const String& str)
    {
        return new CSSCustomIdentValue(str);
    }

    // TODO(sashab, timloh): Remove this and lazily parse the CSSPropertyID in isKnownPropertyID().
    static CSSCustomIdentValue* create(CSSPropertyID id)
    {
        return new CSSCustomIdentValue(id);
    }

    String value() const { ASSERT(!isKnownPropertyID()); return m_string; }
    bool isKnownPropertyID() const { return m_propertyId != CSSPropertyInvalid; }
    CSSPropertyID valueAsPropertyID() const { ASSERT(isKnownPropertyID()); return m_propertyId; }

    String customCSSText() const;

    bool equals(const CSSCustomIdentValue& other) const
    {
        return isKnownPropertyID() ? m_propertyId == other.m_propertyId : m_string == other.m_string;
    }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSCustomIdentValue(const String&);
    CSSCustomIdentValue(CSSPropertyID);

    // TODO(sashab): Change this to an AtomicString.
    String m_string;
    CSSPropertyID m_propertyId;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCustomIdentValue, isCustomIdentValue());

} // namespace blink

#endif // CSSCustomIdentValue_h
