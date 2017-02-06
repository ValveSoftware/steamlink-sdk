// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultSpacing_h
#define ShapeResultSpacing_h

#include "platform/PlatformExport.h"
#include "platform/text/Character.h"

namespace blink {

class FontDescription;
class ShapeResult;
class TextRun;

class PLATFORM_EXPORT ShapeResultSpacing final {
public:
    ShapeResultSpacing(const TextRun&, const FontDescription&);

    float letterSpacing() const { return m_letterSpacing; }
    bool hasSpacing() const { return m_hasSpacing; }
    bool isVerticalOffset() const { return m_isVerticalOffset; }

    float computeSpacing(const TextRun&, size_t, float& offset);

private:
    bool hasExpansion() const { return m_expansionOpportunityCount; }
    bool isAfterExpansion() const { return m_isAfterExpansion; }
    bool isFirstRun(const TextRun&) const;

    float nextExpansion();

    const TextRun& m_textRun;
    float m_letterSpacing;
    float m_wordSpacing;
    float m_expansion;
    float m_expansionPerOpportunity;
    unsigned m_expansionOpportunityCount;
    TextJustify m_textJustify;
    bool m_hasSpacing;
    bool m_normalizeSpace;
    bool m_allowTabs;
    bool m_isAfterExpansion;
    bool m_isVerticalOffset;
};

} // namespace blink

#endif
