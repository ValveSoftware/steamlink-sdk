// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyFunctions_h
#define CSSPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;

// The parent class for all properties that implement the CSSPropertyDescriptor API.
// Contains static function signatures as a reference of what needs to be implemented
// in the child classes.
class CSSPropertyFunctions {
    STATIC_ONLY(CSSPropertyFunctions);
public:
    static CSSPropertyID id();

    // Consumes and parses a single value for this property from by the token range,
    // returning the corresponding CSSValue. This function does not check for the end
    // of the token range. Returns nullptr if the input is invalid.
    static CSSValue* parseSingleValue(CSSParserTokenRange&, const CSSParserContext&);
    using parseSingleValueFunction = decltype(&parseSingleValue);
};

} // namespace blink

#endif // CSSPropertyFunctions_h
