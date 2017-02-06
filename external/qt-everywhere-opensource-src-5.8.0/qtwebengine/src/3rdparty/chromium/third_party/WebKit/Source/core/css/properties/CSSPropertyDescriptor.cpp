// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyDescriptor.h"

namespace blink {

namespace {

// This is the maximum number of properties that can be used at any one time.
const static size_t propertyArraySize = lastCSSProperty + 1;
static CSSPropertyDescriptor cssPropertyDescriptorsById[propertyArraySize];

} // namespace

void CSSPropertyDescriptor::add(CSSPropertyDescriptor descriptor)
{
    ASSERT(descriptor.m_id >= 0 && static_cast<size_t>(descriptor.m_id) < propertyArraySize);
    ASSERT(descriptor.m_valid);
    cssPropertyDescriptorsById[descriptor.m_id] = descriptor;
}

const CSSPropertyDescriptor* CSSPropertyDescriptor::get(CSSPropertyID id)
{
    static bool arrayInitialized = false;
    if (!arrayInitialized) {
        for (size_t i = 0; i < propertyArraySize; ++i)
            cssPropertyDescriptorsById[i].m_valid = false;
        arrayInitialized = true;
    }

    if (cssPropertyDescriptorsById[id].m_valid)
        return &cssPropertyDescriptorsById[id];
    return nullptr;
}

} // namespace blink
