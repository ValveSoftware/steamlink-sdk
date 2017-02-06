// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunk_h
#define PaintChunk_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "wtf/Allocator.h"
#include <iosfwd>

namespace blink {

// A contiguous sequence of drawings with common paint properties.
//
// This is expected to be owned by the paint artifact which also owns the
// related drawings.
//
// This is a Slimming Paint v2 class.
struct PaintChunk {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    PaintChunk() : beginIndex(0), endIndex(0), knownToBeOpaque(false) { }
    PaintChunk(unsigned begin, unsigned end, const PaintChunkProperties& props)
        : beginIndex(begin), endIndex(end), properties(props), knownToBeOpaque(false) { }

    unsigned size() const
    {
        ASSERT(endIndex >= beginIndex);
        return endIndex - beginIndex;
    }

    // Index of the first drawing in this chunk.
    unsigned beginIndex;

    // Index of the first drawing not in this chunk, so that there are
    // |endIndex - beginIndex| drawings in the chunk.
    unsigned endIndex;

    // The paint properties which apply to this chunk.
    PaintChunkProperties properties;

    // The total bounds of this paint chunk's contents.
    FloatRect bounds;

    // True if the bounds are filled entirely with opaque contents.
    bool knownToBeOpaque;
};

inline bool operator==(const PaintChunk& a, const PaintChunk& b)
{
    return a.beginIndex == b.beginIndex
        && a.endIndex == b.endIndex
        && a.properties == b.properties
        && a.bounds == b.bounds
        && a.knownToBeOpaque == b.knownToBeOpaque;
}

inline bool operator!=(const PaintChunk& a, const PaintChunk& b)
{
    return !(a == b);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintChunk&, std::ostream*);

} // namespace blink

#endif // PaintChunk_h
