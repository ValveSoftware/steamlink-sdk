/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V0CustomElementDescriptorHash_h
#define V0CustomElementDescriptorHash_h

#include "core/dom/custom/V0CustomElementDescriptor.h"
#include "wtf/Allocator.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

struct V0CustomElementDescriptorHash {
    STATIC_ONLY(V0CustomElementDescriptorHash);
    static unsigned hash(const V0CustomElementDescriptor& descriptor)
    {
        return WTF::hashInts(AtomicStringHash::hash(descriptor.type()), WTF::hashInts(AtomicStringHash::hash(descriptor.namespaceURI()), AtomicStringHash::hash(descriptor.localName())));
    }

    static bool equal(const V0CustomElementDescriptor& a, const V0CustomElementDescriptor& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace blink

namespace WTF {

template<>
struct HashTraits<blink::V0CustomElementDescriptor> : SimpleClassHashTraits<blink::V0CustomElementDescriptor> {
    STATIC_ONLY(HashTraits);
    static const bool emptyValueIsZero = HashTraits<AtomicString>::emptyValueIsZero;
};

} // namespace WTF

#endif // V0CustomElementDescriptorHash
