// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

namespace blink {

using CharacterPropertyType = uint8_t;

enum class CharacterProperty : CharacterPropertyType {
    isCJKIdeographOrSymbol = 0x0001,
    isUprightInMixedVertical = 0x0002,
    isPotentialCustomElementNameChar = 0x0004,
};

inline CharacterProperty operator | (
    CharacterProperty a, CharacterProperty b)
{
    return static_cast<CharacterProperty>(
        static_cast<CharacterPropertyType>(a)
        | static_cast<CharacterPropertyType>(b));
}

inline CharacterProperty operator & (
    CharacterProperty a, CharacterProperty b)
{
    return static_cast<CharacterProperty>(
        static_cast<CharacterPropertyType>(a)
        & static_cast<CharacterPropertyType>(b));
}

inline CharacterProperty operator |= (
    CharacterProperty& a, CharacterProperty b)
{
    a = a | b;
    return a;
}

} // namespace blink
