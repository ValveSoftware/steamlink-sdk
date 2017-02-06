// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RunSegmenter_h
#define RunSegmenter_h

#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/FontTraits.h"
#include "platform/fonts/OrientationIterator.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/SymbolsIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>
#include <unicode/uscript.h>

namespace blink {

// A tool for segmenting runs prior to shaping, combining ScriptIterator,
// OrientationIterator and SmallCapsIterator, depending on orientaton and
// font-variant of the text run.
class PLATFORM_EXPORT RunSegmenter {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(RunSegmenter);
public:

    // Indices into the UTF-16 buffer that is passed in
    struct RunSegmenterRange {
        DISALLOW_NEW();
        unsigned start;
        unsigned end;
        UScriptCode script;
        OrientationIterator::RenderOrientation renderOrientation;
        FontFallbackPriority fontFallbackPriority;
    };

    RunSegmenter(const UChar* buffer, unsigned bufferSize, FontOrientation);

    bool consume(RunSegmenterRange*);

private:

    void consumeOrientationIteratorPastLastSplit();
    void consumeScriptIteratorPastLastSplit();
    void consumeSymbolsIteratorPastLastSplit();

    unsigned m_bufferSize;
    RunSegmenterRange m_candidateRange;
    std::unique_ptr<ScriptRunIterator> m_scriptRunIterator;
    std::unique_ptr<OrientationIterator> m_orientationIterator;
    std::unique_ptr<SymbolsIterator> m_symbolsIterator;
    unsigned m_lastSplit;
    unsigned m_scriptRunIteratorPosition;
    unsigned m_orientationIteratorPosition;
    unsigned m_symbolsIteratorPosition;
    bool m_atEnd;
};

} // namespace blink

#endif
