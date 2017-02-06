// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGInlineTextBoxPainter.h"

#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/InlineTextBoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/style/ShadowList.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include <memory>

namespace blink {

static inline bool textShouldBePainted(const LayoutSVGInlineText& textLayoutObject)
{
    // Font::pixelSize(), returns FontDescription::computedPixelSize(), which returns "int(x + 0.5)".
    // If the absolute font size on screen is below x=0.5, don't render anything.
    return textLayoutObject.scaledFont().getFontDescription().computedPixelSize();
}

bool SVGInlineTextBoxPainter::shouldPaintSelection(const PaintInfo& paintInfo) const
{
    return !paintInfo.isPrinting() && m_svgInlineTextBox.getSelectionState() != SelectionNone;
}

void SVGInlineTextBoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(m_svgInlineTextBox.truncation() == cNoTruncation);

    if (m_svgInlineTextBox.getLineLayoutItem().style()->visibility() != VISIBLE)
        return;

    // We're explicitly not supporting composition & custom underlines and custom highlighters -- unlike InlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    if (paintInfo.phase == PaintPhaseSelection && !shouldPaintSelection(paintInfo))
        return;

    LayoutSVGInlineText& textLayoutObject = toLayoutSVGInlineText(*LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.getLineLayoutItem()));
    if (!textShouldBePainted(textLayoutObject))
        return;

    DisplayItem::Type displayItemType = DisplayItem::paintPhaseToDrawingType(paintInfo.phase);
    if (!DrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_svgInlineTextBox, displayItemType)) {
        LayoutObject& parentLayoutObject = *LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.parent()->getLineLayoutItem());
        const ComputedStyle& style = parentLayoutObject.styleRef();

        // TODO(chrishtr): passing the cull rect is incorrect.
        DrawingRecorder recorder(paintInfo.context, m_svgInlineTextBox, displayItemType, FloatRect(paintInfo.cullRect().m_rect));
        InlineTextBoxPainter(m_svgInlineTextBox).paintDocumentMarkers(
            paintInfo, paintOffset, style,
            textLayoutObject.scaledFont(), DocumentMarkerPaintPhase::Background);

        if (!m_svgInlineTextBox.textFragments().isEmpty())
            paintTextFragments(paintInfo, parentLayoutObject);

        InlineTextBoxPainter(m_svgInlineTextBox).paintDocumentMarkers(
            paintInfo, paintOffset, style,
            textLayoutObject.scaledFont(), DocumentMarkerPaintPhase::Foreground);
    }
}

void SVGInlineTextBoxPainter::paintTextFragments(const PaintInfo& paintInfo, LayoutObject& parentLayoutObject)
{
    const ComputedStyle& style = parentLayoutObject.styleRef();
    const SVGComputedStyle& svgStyle = style.svgStyle();

    bool hasFill = svgStyle.hasFill();
    bool hasVisibleStroke = svgStyle.hasVisibleStroke();

    const ComputedStyle* selectionStyle = &style;
    bool shouldPaintSelection = this->shouldPaintSelection(paintInfo);
    if (shouldPaintSelection) {
        selectionStyle = parentLayoutObject.getCachedPseudoStyle(PseudoIdSelection);
        if (selectionStyle) {
            const SVGComputedStyle& svgSelectionStyle = selectionStyle->svgStyle();

            if (!hasFill)
                hasFill = svgSelectionStyle.hasFill();
            if (!hasVisibleStroke)
                hasVisibleStroke = svgSelectionStyle.hasVisibleStroke();
        } else {
            selectionStyle = &style;
        }
    }

    if (paintInfo.isRenderingClipPathAsMaskImage()) {
        hasFill = true;
        hasVisibleStroke = false;
    }

    unsigned textFragmentsSize = m_svgInlineTextBox.textFragments().size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_svgInlineTextBox.textFragments().at(i);

        GraphicsContextStateSaver stateSaver(paintInfo.context, false);
        if (fragment.isTransformed()) {
            stateSaver.save();
            paintInfo.context.concatCTM(fragment.buildFragmentTransform());
        }

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        unsigned decorations = style.textDecorationsInEffect();
        if (decorations & TextDecorationUnderline)
            paintDecoration(paintInfo, TextDecorationUnderline, fragment);
        if (decorations & TextDecorationOverline)
            paintDecoration(paintInfo, TextDecorationOverline, fragment);

        for (int i = 0; i < 3; i++) {
            switch (svgStyle.paintOrderType(i)) {
            case PT_FILL:
                if (hasFill)
                    paintText(paintInfo, style, *selectionStyle, fragment, ApplyToFillMode, shouldPaintSelection);
                break;
            case PT_STROKE:
                if (hasVisibleStroke)
                    paintText(paintInfo, style, *selectionStyle, fragment, ApplyToStrokeMode, shouldPaintSelection);
                break;
            case PT_MARKERS:
                // Markers don't apply to text
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & TextDecorationLineThrough)
            paintDecoration(paintInfo, TextDecorationLineThrough, fragment);
    }
}

void SVGInlineTextBoxPainter::paintSelectionBackground(const PaintInfo& paintInfo)
{
    if (m_svgInlineTextBox.getLineLayoutItem().style()->visibility() != VISIBLE)
        return;

    ASSERT(!paintInfo.isPrinting());

    if (paintInfo.phase == PaintPhaseSelection || !shouldPaintSelection(paintInfo))
        return;

    Color backgroundColor = m_svgInlineTextBox.getLineLayoutItem().selectionBackgroundColor();
    if (!backgroundColor.alpha())
        return;

    LayoutSVGInlineText& textLayoutObject = toLayoutSVGInlineText(*LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.getLineLayoutItem()));
    if (!textShouldBePainted(textLayoutObject))
        return;

    const ComputedStyle& style = m_svgInlineTextBox.parent()->getLineLayoutItem().styleRef();

    int startPosition, endPosition;
    m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);

    const Vector<SVGTextFragmentWithRange> fragmentInfoList = collectFragmentsInRange(startPosition, endPosition);
    for (const SVGTextFragmentWithRange& fragmentWithRange : fragmentInfoList) {
        const SVGTextFragment& fragment = fragmentWithRange.fragment;
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        if (fragment.isTransformed())
            paintInfo.context.concatCTM(fragment.buildFragmentTransform());

        paintInfo.context.setFillColor(backgroundColor);
        paintInfo.context.fillRect(m_svgInlineTextBox.selectionRectForTextFragment(fragment, fragmentWithRange.startPosition, fragmentWithRange.endPosition, style), backgroundColor);
    }
}

static inline LayoutObject* findLayoutObjectDefininingTextDecoration(InlineFlowBox* parentBox)
{
    // Lookup first layout object in parent hierarchy which has text-decoration set.
    LayoutObject* layoutObject = 0;
    while (parentBox) {
        layoutObject = LineLayoutAPIShim::layoutObjectFrom(parentBox->getLineLayoutItem());

        if (layoutObject->style() && layoutObject->style()->getTextDecoration() != TextDecorationNone)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(layoutObject);
    return layoutObject;
}

// Offset from the baseline for |decoration|. Positive offsets are above the baseline.
static inline float baselineOffsetForDecoration(TextDecoration decoration, const FontMetrics& fontMetrics, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Presto.
    if (decoration == TextDecorationUnderline)
        return -thickness * 1.5f;
    if (decoration == TextDecorationOverline)
        return fontMetrics.floatAscent() - thickness;
    if (decoration == TextDecorationLineThrough)
        return fontMetrics.floatAscent() * 3 / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(TextDecoration, const Font& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Presto
    return font.getFontDescription().computedSize() / 20.0f;
}

void SVGInlineTextBoxPainter::paintDecoration(const PaintInfo& paintInfo, TextDecoration decoration, const SVGTextFragment& fragment)
{
    if (m_svgInlineTextBox.getLineLayoutItem().style()->textDecorationsInEffect() == TextDecorationNone)
        return;

    if (fragment.width <= 0)
        return;

    // Find out which style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    LayoutObject* decorationLayoutObject = findLayoutObjectDefininingTextDecoration(m_svgInlineTextBox.parent());
    const ComputedStyle& decorationStyle = decorationLayoutObject->styleRef();

    if (decorationStyle.visibility() != VISIBLE)
        return;

    float scalingFactor = 1;
    Font scaledFont;
    LayoutSVGInlineText::computeNewScaledFontForStyle(decorationLayoutObject, scalingFactor, scaledFont);
    ASSERT(scalingFactor);

    float thickness = thicknessForDecoration(decoration, scaledFont);
    if (thickness <= 0)
        return;

    float decorationOffset = baselineOffsetForDecoration(decoration, scaledFont.getFontMetrics(), thickness);
    FloatPoint decorationOrigin(fragment.x, fragment.y - decorationOffset / scalingFactor);

    Path path;
    path.addRect(FloatRect(decorationOrigin, FloatSize(fragment.width, thickness / scalingFactor)));

    const SVGComputedStyle& svgDecorationStyle = decorationStyle.svgStyle();

    for (int i = 0; i < 3; i++) {
        switch (svgDecorationStyle.paintOrderType(i)) {
        case PT_FILL:
            if (svgDecorationStyle.hasFill()) {
                SkPaint fillPaint;
                if (!SVGPaintContext::paintForLayoutObject(paintInfo, decorationStyle, *decorationLayoutObject, ApplyToFillMode, fillPaint))
                    break;
                fillPaint.setAntiAlias(true);
                paintInfo.context.drawPath(path.getSkPath(), fillPaint);
            }
            break;
        case PT_STROKE:
            if (svgDecorationStyle.hasVisibleStroke()) {
                SkPaint strokePaint;
                if (!SVGPaintContext::paintForLayoutObject(paintInfo, decorationStyle, *decorationLayoutObject, ApplyToStrokeMode, strokePaint))
                    break;
                strokePaint.setAntiAlias(true);
                StrokeData strokeData;
                SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, decorationStyle, *decorationLayoutObject, 1);
                if (svgDecorationStyle.vectorEffect() == VE_NON_SCALING_STROKE)
                    strokeData.setThickness(strokeData.thickness() / scalingFactor);
                strokeData.setupPaint(&strokePaint);
                paintInfo.context.drawPath(path.getSkPath(), strokePaint);
            }
            break;
        case PT_MARKERS:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

bool SVGInlineTextBoxPainter::setupTextPaint(const PaintInfo& paintInfo, const ComputedStyle& style,
    LayoutSVGResourceMode resourceMode, SkPaint& paint)
{
    LayoutSVGInlineText& textLayoutObject = toLayoutSVGInlineText(*LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.getLineLayoutItem()));

    float scalingFactor = textLayoutObject.scalingFactor();
    ASSERT(scalingFactor);

    const ShadowList* shadowList = style.textShadow();

    // Text shadows are disabled when printing. http://crbug.com/258321
    bool hasShadow = shadowList && !paintInfo.isPrinting();

    AffineTransform paintServerTransform;
    const AffineTransform* additionalPaintServerTransform = 0;

    if (scalingFactor != 1) {
        // Adjust the paint-server coordinate space.
        paintServerTransform.scale(scalingFactor);
        additionalPaintServerTransform = &paintServerTransform;
    }

    if (!SVGPaintContext::paintForLayoutObject(paintInfo, style, *LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.parent()->getLineLayoutItem()), resourceMode, paint, additionalPaintServerTransform))
        return false;
    paint.setAntiAlias(true);

    if (hasShadow) {
        std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = shadowList->createDrawLooper(DrawLooperBuilder::ShadowRespectsAlpha, style.visitedDependentColor(CSSPropertyColor));
        paint.setLooper(toSkSp(drawLooperBuilder->detachDrawLooper()));
    }

    if (resourceMode == ApplyToStrokeMode) {
        StrokeData strokeData;
        SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, style, *LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.parent()->getLineLayoutItem()), 1);
        if (style.svgStyle().vectorEffect() != VE_NON_SCALING_STROKE)
            strokeData.setThickness(strokeData.thickness() * scalingFactor);
        strokeData.setupPaint(&paint);
    }
    return true;
}

void SVGInlineTextBoxPainter::paintText(const PaintInfo& paintInfo, TextRun& textRun, const SVGTextFragment& fragment, int startPosition, int endPosition, const SkPaint& paint)
{
    LayoutSVGInlineText& textLayoutObject = toLayoutSVGInlineText(*LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.getLineLayoutItem()));
    const Font& scaledFont = textLayoutObject.scaledFont();

    float scalingFactor = textLayoutObject.scalingFactor();
    ASSERT(scalingFactor);

    FloatPoint textOrigin(fragment.x, fragment.y);
    FloatSize textSize(fragment.width, fragment.height);

    GraphicsContext& context = paintInfo.context;
    GraphicsContextStateSaver stateSaver(context, false);
    if (scalingFactor != 1) {
        textOrigin.scale(scalingFactor, scalingFactor);
        textSize.scale(scalingFactor);
        stateSaver.save();
        context.scale(1 / scalingFactor, 1 / scalingFactor);
    }

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.from = startPosition;
    textRunPaintInfo.to = endPosition;

    float baseline = scaledFont.getFontMetrics().floatAscent();
    textRunPaintInfo.bounds = FloatRect(textOrigin.x(), textOrigin.y() - baseline,
        textSize.width(), textSize.height());

    context.drawText(scaledFont, textRunPaintInfo, textOrigin, paint);
}

void SVGInlineTextBoxPainter::paintText(const PaintInfo& paintInfo, const ComputedStyle& style,
    const ComputedStyle& selectionStyle, const SVGTextFragment& fragment,
    LayoutSVGResourceMode resourceMode, bool shouldPaintSelection)
{
    int startPosition = 0;
    int endPosition = 0;
    if (shouldPaintSelection) {
        m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);
        shouldPaintSelection = m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
    }

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun = m_svgInlineTextBox.constructTextRun(style, fragment);
    if (!shouldPaintSelection || startPosition >= endPosition) {
        SkPaint paint;
        if (setupTextPaint(paintInfo, style, resourceMode, paint))
            paintText(paintInfo, textRun, fragment, 0, fragment.length, paint);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    if (startPosition > 0 && !paintSelectedTextOnly) {
        SkPaint paint;
        if (setupTextPaint(paintInfo, style, resourceMode, paint))
            paintText(paintInfo, textRun, fragment, 0, startPosition, paint);
    }

    // Draw text using selection style from the start to the end position of the selection
    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsPaintInvalidationObject();
        SVGResourcesCache::clientStyleChanged(LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.parent()->getLineLayoutItem()), diff, selectionStyle);
    }

    SkPaint paint;
    if (setupTextPaint(paintInfo, selectionStyle, resourceMode, paint))
        paintText(paintInfo, textRun, fragment, startPosition, endPosition, paint);

    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsPaintInvalidationObject();
        SVGResourcesCache::clientStyleChanged(LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.parent()->getLineLayoutItem()), diff, style);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < static_cast<int>(fragment.length) && !paintSelectedTextOnly) {
        SkPaint paint;
        if (setupTextPaint(paintInfo, style, resourceMode, paint))
            paintText(paintInfo, textRun, fragment, endPosition, fragment.length, paint);
    }
}

Vector<SVGTextFragmentWithRange> SVGInlineTextBoxPainter::collectTextMatches(DocumentMarker* marker) const
{
    const Vector<SVGTextFragmentWithRange> emptyTextMatchList;

    // SVG does not support grammar or spellcheck markers, so skip anything but TextMatch.
    if (marker->type() != DocumentMarker::TextMatch)
        return emptyTextMatchList;

    if (!LineLayoutAPIShim::layoutObjectFrom(m_svgInlineTextBox.getLineLayoutItem())->frame()->editor().markedTextMatchesAreHighlighted())
        return emptyTextMatchList;

    int markerStartPosition = std::max<int>(marker->startOffset() - m_svgInlineTextBox.start(), 0);
    int markerEndPosition = std::min<int>(marker->endOffset() - m_svgInlineTextBox.start(), m_svgInlineTextBox.len());

    if (markerStartPosition >= markerEndPosition)
        return emptyTextMatchList;

    return collectFragmentsInRange(markerStartPosition, markerEndPosition);
}

Vector<SVGTextFragmentWithRange> SVGInlineTextBoxPainter::collectFragmentsInRange(int startPosition, int endPosition) const
{
    Vector<SVGTextFragmentWithRange> fragmentInfoList;
    const Vector<SVGTextFragment>& fragments = m_svgInlineTextBox.textFragments();
    for (const SVGTextFragment& fragment : fragments) {
        // TODO(ramya.v): If these can't be negative we should use unsigned.
        int fragmentStartPosition = startPosition;
        int fragmentEndPosition = endPosition;
        if (!m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        fragmentInfoList.append(SVGTextFragmentWithRange(fragment, fragmentStartPosition, fragmentEndPosition));
    }
    return fragmentInfoList;
}

void SVGInlineTextBoxPainter::paintTextMatchMarkerForeground(const PaintInfo& paintInfo, const LayoutPoint& point, DocumentMarker* marker, const ComputedStyle& style, const Font& font)
{
    const Vector<SVGTextFragmentWithRange> textMatchInfoList = collectTextMatches(marker);
    if (textMatchInfoList.isEmpty())
        return;

    Color textColor = LayoutTheme::theme().platformTextSearchColor(marker->activeMatch());

    SkPaint fillPaint;
    fillPaint.setColor(textColor.rgb());
    fillPaint.setAntiAlias(true);

    SkPaint strokePaint;
    bool shouldPaintStroke  = false;
    if (setupTextPaint(paintInfo, style, ApplyToStrokeMode, strokePaint)) {
        shouldPaintStroke = true;
        strokePaint.setLooper(nullptr);
        strokePaint.setColor(textColor.rgb());
    }

    for (const SVGTextFragmentWithRange& textMatchInfo : textMatchInfoList) {
        const SVGTextFragment& fragment = textMatchInfo.fragment;
        GraphicsContextStateSaver stateSaver(paintInfo.context);
        if (fragment.isTransformed())
            paintInfo.context.concatCTM(fragment.buildFragmentTransform());

        TextRun textRun = m_svgInlineTextBox.constructTextRun(style, fragment);
        paintText(paintInfo, textRun, fragment, textMatchInfo.startPosition, textMatchInfo.endPosition, fillPaint);
        if (shouldPaintStroke)
            paintText(paintInfo, textRun, fragment, textMatchInfo.startPosition, textMatchInfo.endPosition, strokePaint);
    }
}

void SVGInlineTextBoxPainter::paintTextMatchMarkerBackground(const PaintInfo& paintInfo, const LayoutPoint& point, DocumentMarker* marker, const ComputedStyle& style, const Font& font)
{
    const Vector<SVGTextFragmentWithRange> textMatchInfoList = collectTextMatches(marker);
    if (textMatchInfoList.isEmpty())
        return;

    Color color = LayoutTheme::theme().platformTextSearchHighlightColor(marker->activeMatch());
    for (const SVGTextFragmentWithRange& textMatchInfo : textMatchInfoList) {
        const SVGTextFragment& fragment = textMatchInfo.fragment;

        GraphicsContextStateSaver stateSaver(paintInfo.context, false);
        if (fragment.isTransformed()) {
            stateSaver.save();
            paintInfo.context.concatCTM(fragment.buildFragmentTransform());
        }
        FloatRect fragmentRect = m_svgInlineTextBox.selectionRectForTextFragment(fragment, textMatchInfo.startPosition, textMatchInfo.endPosition, style);
        paintInfo.context.setFillColor(color);
        paintInfo.context.fillRect(fragmentRect);
    }
}

} // namespace blink
