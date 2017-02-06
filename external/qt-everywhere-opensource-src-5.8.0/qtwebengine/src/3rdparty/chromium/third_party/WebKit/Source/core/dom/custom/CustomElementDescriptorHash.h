// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDescriptorHash_h
#define CustomElementDescriptorHash_h

#include "core/dom/custom/CustomElementDescriptor.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

struct CustomElementDescriptorHash {
    STATIC_ONLY(CustomElementDescriptorHash);
    static unsigned hash(const CustomElementDescriptor& descriptor)
    {
        return WTF::hashInts(
            AtomicStringHash::hash(descriptor.name()),
            AtomicStringHash::hash(descriptor.localName()));
    }

    static bool equal(
        const CustomElementDescriptor& a,
        const CustomElementDescriptor& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace blink

namespace WTF {

template<>
struct HashTraits<blink::CustomElementDescriptor>
    : SimpleClassHashTraits<blink::CustomElementDescriptor> {
    STATIC_ONLY(HashTraits);
    static const bool emptyValueIsZero =
        HashTraits<AtomicString>::emptyValueIsZero;
};

template<>
struct DefaultHash<blink::CustomElementDescriptor> {
    using Hash = blink::CustomElementDescriptorHash;
};

} // namespace WTF

#endif // CustomElementDescriptorHash_h
