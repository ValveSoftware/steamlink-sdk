// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CaseMappingHarfBuzzBufferFiller_h
#define CaseMappingHarfBuzzBufferFiller_h

#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/Unicode.h"

#include <hb.h>

namespace blink {

enum class CaseMapIntend {
    KeepSameCase,
    UpperCase,
    LowerCase
};

class CaseMappingHarfBuzzBufferFiller {
    STACK_ALLOCATED()

public:
    CaseMappingHarfBuzzBufferFiller(
        CaseMapIntend,
        AtomicString locale,
        hb_buffer_t* harfBuzzBuffer,
        const UChar* buffer,
        unsigned bufferLength,
        unsigned startIndex,
        unsigned numCharacters);

private:
    void fillSlowCase(CaseMapIntend,
        AtomicString locale,
        const UChar* buffer,
        unsigned bufferLength,
        unsigned startIndex,
        unsigned numCharacters);
    hb_buffer_t* m_harfBuzzBuffer;
};

} // namespace blink

#endif
