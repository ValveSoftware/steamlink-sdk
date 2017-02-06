// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutSVGInlineText_h
#define LineLayoutSVGInlineText_h

#include "core/layout/api/LineLayoutText.h"
#include "core/layout/svg/LayoutSVGInlineText.h"

namespace blink {

class LineLayoutSVGInlineText : public LineLayoutText {
public:
    explicit LineLayoutSVGInlineText(LayoutSVGInlineText* layoutSVGInlineText)
        : LineLayoutText(layoutSVGInlineText)
    {
    }

    explicit LineLayoutSVGInlineText(const LineLayoutItem& item)
        : LineLayoutText(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isSVGInlineText());
    }

    explicit LineLayoutSVGInlineText(std::nullptr_t) : LineLayoutText(nullptr) { }

    LineLayoutSVGInlineText() { }

    const Vector<SVGTextMetrics>& metricsList() const
    {
        return toSVGInlineText()->metricsList();
    }

    SVGCharacterDataMap& characterDataMap()
    {
        return toSVGInlineText()->characterDataMap();
    }

    bool characterStartsNewTextChunk(int position) const
    {
        return toSVGInlineText()->characterStartsNewTextChunk(position);
    }

    float scalingFactor() const
    {
        return toSVGInlineText()->scalingFactor();
    }

    const Font& scaledFont() const
    {
        return toSVGInlineText()->scaledFont();
    }

private:
    LayoutSVGInlineText* toSVGInlineText()
    {
        return toLayoutSVGInlineText(layoutObject());
    }

    const LayoutSVGInlineText* toSVGInlineText() const
    {
        return toLayoutSVGInlineText(layoutObject());
    }
};

class SVGInlineTextMetricsIterator {
    DISALLOW_NEW();
public:
    SVGInlineTextMetricsIterator() { reset(LineLayoutSVGInlineText()); }
    explicit SVGInlineTextMetricsIterator(LineLayoutSVGInlineText textLineLayout) { reset(textLineLayout); }

    void advanceToTextStart(LineLayoutSVGInlineText textLineLayout, unsigned startCharacterOffset)
    {
        ASSERT(textLineLayout);
        if (!m_textLineLayout || m_textLineLayout != textLineLayout) {
            reset(textLineLayout);
            ASSERT(!metricsList().isEmpty());
        }

        if (m_characterOffset == startCharacterOffset)
            return;

        // TODO(fs): We could walk backwards through the metrics list in these cases.
        if (m_characterOffset > startCharacterOffset)
            reset(textLineLayout);

        while (m_characterOffset < startCharacterOffset)
            next();
        ASSERT(m_characterOffset == startCharacterOffset);
    }

    void next()
    {
        m_characterOffset += metrics().length();
        ASSERT(m_characterOffset <= m_textLineLayout.length());
        ASSERT(m_metricsListOffset < metricsList().size());
        ++m_metricsListOffset;
    }

    const SVGTextMetrics& metrics() const
    {
        ASSERT(m_textLineLayout && m_metricsListOffset < metricsList().size());
        return metricsList()[m_metricsListOffset];
    }
    const Vector<SVGTextMetrics>& metricsList() const { return m_textLineLayout.metricsList(); }
    unsigned metricsListOffset() const { return m_metricsListOffset; }
    unsigned characterOffset() const { return m_characterOffset; }
    bool isAtEnd() const { return m_metricsListOffset == metricsList().size(); }

private:
    void reset(LineLayoutSVGInlineText textLineLayout)
    {
        m_textLineLayout = textLineLayout;
        m_characterOffset = 0;
        m_metricsListOffset = 0;
    }

    LineLayoutSVGInlineText m_textLineLayout;
    unsigned m_metricsListOffset;
    unsigned m_characterOffset;
};

} // namespace blink

#endif // LineLayoutSVGInlineText_h
