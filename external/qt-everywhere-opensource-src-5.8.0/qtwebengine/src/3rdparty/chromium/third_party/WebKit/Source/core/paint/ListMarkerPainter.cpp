// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ListMarkerPainter.h"

#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/ListMarkerText.h"
#include "core/layout/api/SelectionState.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainter.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

static inline void paintSymbol(GraphicsContext& context, const Color& color,
    const IntRect& marker, EListStyleType listStyle)
{
    context.setStrokeColor(color);
    context.setStrokeStyle(SolidStroke);
    context.setStrokeThickness(1.0f);
    switch (listStyle) {
    case Disc:
        context.fillEllipse(marker);
        break;
    case Circle:
        context.strokeEllipse(marker);
        break;
    case Square:
        context.fillRect(marker);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void ListMarkerPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground)
        return;

    if (m_layoutListMarker.style()->visibility() != VISIBLE)
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutListMarker, paintInfo.phase))
        return;

    LayoutPoint boxOrigin(paintOffset + m_layoutListMarker.location());
    LayoutRect overflowRect(m_layoutListMarker.visualOverflowRect());
    overflowRect.moveBy(boxOrigin);

    IntRect pixelSnappedOverflowRect = pixelSnappedIntRect(overflowRect);
    if (!paintInfo.cullRect().intersectsCullRect(overflowRect))
        return;

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutListMarker, paintInfo.phase, pixelSnappedOverflowRect);

    LayoutRect box(boxOrigin, m_layoutListMarker.size());

    IntRect marker = m_layoutListMarker.getRelativeMarkerRect();
    marker.moveBy(roundedIntPoint(boxOrigin));

    GraphicsContext& context = paintInfo.context;

    if (m_layoutListMarker.isImage()) {
        context.drawImage(m_layoutListMarker.image()->image(
            m_layoutListMarker, marker.size(), m_layoutListMarker.styleRef().effectiveZoom()).get(), marker);
        if (m_layoutListMarker.getSelectionState() != SelectionNone) {
            LayoutRect selRect = m_layoutListMarker.localSelectionRect();
            selRect.moveBy(boxOrigin);
            context.fillRect(pixelSnappedIntRect(selRect), m_layoutListMarker.listItem()->selectionBackgroundColor());
        }
        return;
    }

    LayoutListMarker::ListStyleCategory styleCategory = m_layoutListMarker.getListStyleCategory();
    if (styleCategory == LayoutListMarker::ListStyleCategory::None)
        return;

    Color color(m_layoutListMarker.resolveColor(CSSPropertyColor));

    if (BoxPainter::shouldForceWhiteBackgroundForPrintEconomy(m_layoutListMarker.styleRef(), m_layoutListMarker.listItem()->document()))
        color = TextPainter::textColorForWhiteBackground(color);

    // Apply the color to the list marker text.
    context.setFillColor(color);

    const EListStyleType listStyle = m_layoutListMarker.style()->listStyleType();
    if (styleCategory == LayoutListMarker::ListStyleCategory::Symbol) {
        paintSymbol(context, color, marker, listStyle);
        return;
    }

    if (m_layoutListMarker.text().isEmpty())
        return;

    const Font& font = m_layoutListMarker.style()->font();
    TextRun textRun = constructTextRun(font, m_layoutListMarker.text(), m_layoutListMarker.styleRef());

    GraphicsContextStateSaver stateSaver(context, false);
    if (!m_layoutListMarker.style()->isHorizontalWritingMode()) {
        marker.moveBy(roundedIntPoint(-boxOrigin));
        marker = marker.transposedRect();
        marker.moveBy(IntPoint(roundToInt(box.x()), roundToInt(box.y() - m_layoutListMarker.logicalHeight())));
        stateSaver.save();
        context.translate(marker.x(), marker.maxY());
        context.rotate(static_cast<float>(deg2rad(90.)));
        context.translate(-marker.x(), -marker.maxY());
    }

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = marker;
    IntPoint textOrigin = IntPoint(marker.x(), marker.y() + m_layoutListMarker.style()->getFontMetrics().ascent());

    // Text is not arbitrary. We can judge whether it's RTL from the first character,
    // and we only need to handle the direction RightToLeft for now.
    bool textNeedsReversing = WTF::Unicode::direction(m_layoutListMarker.text()[0]) == WTF::Unicode::RightToLeft;
    StringBuilder reversedText;
    if (textNeedsReversing) {
        unsigned length = m_layoutListMarker.text().length();
        reversedText.reserveCapacity(length);
        for (int i = length - 1; i >= 0; --i)
            reversedText.append(m_layoutListMarker.text()[i]);
        ASSERT(reversedText.length() == length);
        textRun.setText(reversedText.toString());
    }

    const UChar suffix = ListMarkerText::suffix(listStyle, m_layoutListMarker.listItem()->value());
    UChar suffixStr[2] = { suffix, static_cast<UChar>(' ') };
    TextRun suffixRun = constructTextRun(font, suffixStr, 2, m_layoutListMarker.styleRef(), m_layoutListMarker.style()->direction());
    TextRunPaintInfo suffixRunInfo(suffixRun);
    suffixRunInfo.bounds = marker;

    if (m_layoutListMarker.style()->isLeftToRightDirection()) {
        context.drawText(font, textRunPaintInfo, textOrigin);
        context.drawText(font, suffixRunInfo, textOrigin + IntSize(font.width(textRun), 0));
    } else {
        context.drawText(font, suffixRunInfo, textOrigin);
        context.drawText(font, textRunPaintInfo, textOrigin + IntSize(font.width(suffixRun), 0));
    }
}

} // namespace blink
