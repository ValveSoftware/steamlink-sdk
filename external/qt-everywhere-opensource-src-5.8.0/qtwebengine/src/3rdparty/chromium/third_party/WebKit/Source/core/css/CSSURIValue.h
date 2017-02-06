// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURIValue_h
#define CSSURIValue_h

#include "core/css/CSSValue.h"

namespace blink {

class CSSURIValue : public CSSValue {
public:
    static CSSURIValue* create(const String& str)
    {
        return new CSSURIValue(str);
    }

    String value() const { return m_string; }

    String customCSSText() const;

    bool equals(const CSSURIValue& other) const
    {
        return m_string == other.m_string;
    }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSURIValue(const String&);

    String m_string;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSURIValue, isURIValue());

} // namespace blink

#endif // CSSURIValue_h
