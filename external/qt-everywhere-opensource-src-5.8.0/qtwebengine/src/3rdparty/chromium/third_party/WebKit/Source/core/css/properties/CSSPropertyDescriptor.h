// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyDescriptor_h
#define CSSPropertyDescriptor_h

#include "core/css/properties/CSSPropertyFunctions.h"
#include <type_traits>

namespace blink {

/**
 * The API for a single, non-shorthand property in the CSS style engine. All properties
 * in the style engine must implement this API (see CSSPropertyFunctions).
 *
 * The API is a complete, stateless representation of all the behaviors of a single
 * property (e.g. 'width', 'color'). Calls to property logic are directed through the
 * CSSPropertyDescriptor object, which stores resolved function pointers.
 *
 * To add a new property, create a subclass of CSSPropertyFunctions and implement
 * all public static methods. Then, call CSSPropertyDescriptor::create<SubclassName>() to get a
 * descriptor for the class, which can be registered with CSSPropertyDescriptor::add().
 *
 * No property-specific logic should exist in the style engine outside of this class.
 */
class CSSPropertyDescriptor {
    STACK_ALLOCATED();
public:
    // Given a subclass of CSSPropertyFunctions, creates a CSSPropertyDescriptor from its static methods.
    template <class T> static CSSPropertyDescriptor create()
    {
        static_assert(std::is_base_of<T, CSSPropertyFunctions>::value, "Property must inherit from CSSPropertyFunctions");
        ASSERT(T::id != CSSPropertyFunctions::id);
        ASSERT(T::parseSingleValue != CSSPropertyFunctions::parseSingleValue);
        CSSPropertyDescriptor descriptor;
        descriptor.m_valid = true;
        descriptor.m_id = T::id();
        descriptor.m_parseSingleValue = T::parseSingleValue;
        return descriptor;
    }

    static void add(CSSPropertyDescriptor);

    // Returns the property's descriptor, or null if that property does not have an API yet.
    // TODO(sashab): Change this to return a const& once all properties have an API.
    static const CSSPropertyDescriptor* get(CSSPropertyID);

    // Accessors to functions on the property API.
    CSSPropertyID id() const { return m_id; }
    CSSValue* parseSingleValue(CSSParserTokenRange& range, const CSSParserContext& context) const { return m_parseSingleValue(range, context); }

private:
    // Used internally to check whether an array entry is filled or not.
    bool m_valid;

    CSSPropertyID m_id;
    CSSPropertyFunctions::parseSingleValueFunction m_parseSingleValue;
};

} // namespace blink

#endif // CSSPropertyDescriptor_h
