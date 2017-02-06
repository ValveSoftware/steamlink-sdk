/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2011 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGParsingError_h
#define SVGParsingError_h

#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

class QualifiedName;

enum class SVGParseStatus {
    NoError,

    // Syntax errors
    TrailingGarbage,
    ExpectedAngle,
    ExpectedArcFlag,
    ExpectedBoolean,
    ExpectedEndOfArguments,
    ExpectedEnumeration,
    ExpectedInteger,
    ExpectedLength,
    ExpectedMoveToCommand,
    ExpectedNumber,
    ExpectedNumberOrPercentage,
    ExpectedPathCommand,
    ExpectedStartOfArguments,
    ExpectedTransformFunction,

    // Semantic errors
    NegativeValue,
    ZeroValue,

    // Generic error
    ParsingFailed,
};

class SVGParsingError {
    STACK_ALLOCATED();
public:
    SVGParsingError(SVGParseStatus status = SVGParseStatus::NoError, size_t locus = 0)
        : m_status(static_cast<unsigned>(status))
        , m_locus(checkLocus(locus))
    {
        ASSERT(this->status() == status);
    }

    SVGParseStatus status() const { return static_cast<SVGParseStatus>(m_status); }

    bool hasLocus() const { return m_locus != kNoLocus; }
    unsigned locus() const { return m_locus; }

    // Move the locus of this error by |offset|, returning in a new error.
    SVGParsingError offsetWith(size_t offset) const { return SVGParsingError(status(), offset + locus()); }

    // Generates a string describing this error for |value| in the context of
    // an <element, attribute>-name pair.
    String format(const String& tagName, const QualifiedName&, const AtomicString& value) const;

private:
    static const int kLocusBits = 24;
    static const unsigned kNoLocus = (1u << kLocusBits) - 1;

    static unsigned checkLocus(size_t locus)
    {
        // Clamp to fit in the number of bits available. If the character index
        // encoded by the locus does not fit in the number of bits allocated
        // for it, the locus will be disabled (set to kNoLocus). This means
        // that very long values will be output in their entirety. That should
        // however be rather uncommon.
        return clampTo<unsigned>(locus, 0, kNoLocus);
    }

    unsigned m_status : 8;
    unsigned m_locus : kLocusBits; // The locus (character index) of the error within the parsed string.
};

inline bool operator==(const SVGParsingError& error, SVGParseStatus status)
{
    return error.status() == status;
}
inline bool operator!=(const SVGParsingError& error, SVGParseStatus status) { return !(error == status); }

} // namespace blink

#endif // SVGParsingError_h
