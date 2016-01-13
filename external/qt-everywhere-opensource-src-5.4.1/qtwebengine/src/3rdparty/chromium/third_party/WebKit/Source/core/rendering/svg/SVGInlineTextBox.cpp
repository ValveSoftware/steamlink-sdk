/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/rendering/svg/SVGInlineTextBox.h"

#include "core/dom/DocumentMarkerController.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/editing/Editor.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/InlineFlowBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/PointerEventsHitRules.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGResourceSolidColor.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/rendering/svg/SVGTextRunRenderingContext.h"
#include "platform/FloatConversion.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

using namespace std;

namespace WebCore {

struct ExpectedSVGInlineTextBoxSize : public InlineTextBox {
    float float1;
    uint32_t bitfields : 1;
    void* pointer;
    Vector<SVGTextFragment> vector;
};

COMPILE_ASSERT(sizeof(SVGInlineTextBox) == sizeof(ExpectedSVGInlineTextBoxSize), SVGInlineTextBox_is_not_of_expected_size);

SVGInlineTextBox::SVGInlineTextBox(RenderObject& object)
    : InlineTextBox(object)
    , m_logicalHeight(0)
    , m_startsNewTextChunk(false)
    , m_paintingResource(0)
{
}

void SVGInlineTextBox::dirtyLineBoxes()
{
    InlineTextBox::dirtyLineBoxes();

    // Clear the now stale text fragments
    clearTextFragments();

    // And clear any following text fragments as the text on which they
    // depend may now no longer exist, or glyph positions may be wrong
    InlineTextBox* nextBox = nextTextBox();
    if (nextBox)
        nextBox->dirtyLineBoxes();
}

int SVGInlineTextBox::offsetForPosition(float, bool) const
{
    // SVG doesn't use the standard offset <-> position selection system, as it's not suitable for SVGs complex needs.
    // vertical text selection, inline boxes spanning multiple lines (contrary to HTML, etc.)
    ASSERT_NOT_REACHED();
    return 0;
}

int SVGInlineTextBox::offsetForPositionInFragment(const SVGTextFragment& fragment, float position, bool includePartialGlyphs) const
{
    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    RenderStyle* style = textRenderer.style();
    ASSERT(style);

    TextRun textRun = constructTextRun(style, fragment);

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Handle vertical text.
    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform);
    if (!fragmentTransform.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(fragmentTransform.xScale()));

    return fragment.characterOffset - start() + textRenderer.scaledFont().offsetForPosition(textRun, position * scalingFactor, includePartialGlyphs);
}

float SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system.
    ASSERT_NOT_REACHED();
    return 0;
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(const SVGTextFragment& fragment, int startPosition, int endPosition, RenderStyle* style)
{
    ASSERT(startPosition < endPosition);
    ASSERT(style);

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textRenderer.scaledFont();
    const FontMetrics& scaledFontMetrics = scaledFont.fontMetrics();
    FloatPoint textOrigin(fragment.x, fragment.y);
    if (scalingFactor != 1)
        textOrigin.scale(scalingFactor, scalingFactor);

    textOrigin.move(0, -scaledFontMetrics.floatAscent());

    FloatRect selectionRect = scaledFont.selectionRectForText(constructTextRun(style, fragment), textOrigin, fragment.height * scalingFactor, startPosition, endPosition);
    if (scalingFactor == 1)
        return selectionRect;

    selectionRect.scale(1 / scalingFactor);
    return selectionRect;
}

LayoutRect SVGInlineTextBox::localSelectionRect(int startPosition, int endPosition)
{
    int boxStart = start();
    startPosition = max(startPosition - boxStart, 0);
    endPosition = min(endPosition - boxStart, static_cast<int>(len()));
    if (startPosition >= endPosition)
        return LayoutRect();

    RenderStyle* style = textRenderer().style();
    ASSERT(style);

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

static inline bool textShouldBePainted(RenderSVGInlineText& textRenderer)
{
    // Font::pixelSize(), returns FontDescription::computedPixelSize(), which returns "int(x + 0.5)".
    // If the absolute font size on screen is below x=0.5, don't render anything.
    return textRenderer.scaledFont().fontDescription().computedPixelSize();
}

void SVGInlineTextBox::paintSelectionBackground(PaintInfo& paintInfo)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(&renderer()));
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(truncation() == cNoTruncation);

    if (renderer().style()->visibility() != VISIBLE)
        return;

    RenderObject& parentRenderer = parent()->renderer();
    ASSERT(!parentRenderer.document().printing());

    // Determine whether or not we're selected.
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    bool hasSelection = selectionState() != RenderObject::SelectionNone;
    if (!hasSelection || paintSelectedTextOnly)
        return;

    Color backgroundColor = renderer().selectionBackgroundColor();
    if (!backgroundColor.alpha())
        return;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());
    if (!textShouldBePainted(textRenderer))
        return;

    RenderStyle* style = parentRenderer.style();
    ASSERT(style);

    int startPosition, endPosition;
    selectionStartEnd(startPosition, endPosition);

    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;
    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_paintingResource);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        GraphicsContextStateSaver stateSaver(*paintInfo.context);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            paintInfo.context->concatCTM(fragmentTransform);

        paintInfo.context->setFillColor(backgroundColor);
        paintInfo.context->fillRect(selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style), backgroundColor);
    }

    ASSERT(!m_paintingResource);
}

void SVGInlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(&renderer()));
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(truncation() == cNoTruncation);

    if (renderer().style()->visibility() != VISIBLE)
        return;

    // Note: We're explicitely not supporting composition & custom underlines and custom highlighters - unlike InlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    RenderObject& parentRenderer = parent()->renderer();

    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    bool hasSelection = !parentRenderer.document().printing() && selectionState() != RenderObject::SelectionNone;
    if (!hasSelection && paintSelectedTextOnly)
        return;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());
    if (!textShouldBePainted(textRenderer))
        return;

    RenderStyle* style = parentRenderer.style();
    ASSERT(style);

    paintDocumentMarkers(paintInfo.context, paintOffset, style, textRenderer.scaledFont(), true);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    bool hasFill = svgStyle->hasFill();
    bool hasVisibleStroke = svgStyle->hasVisibleStroke();

    RenderStyle* selectionStyle = style;
    if (hasSelection) {
        selectionStyle = parentRenderer.getCachedPseudoStyle(SELECTION);
        if (selectionStyle) {
            const SVGRenderStyle* svgSelectionStyle = selectionStyle->svgStyle();
            ASSERT(svgSelectionStyle);

            if (!hasFill)
                hasFill = svgSelectionStyle->hasFill();
            if (!hasVisibleStroke)
                hasVisibleStroke = svgSelectionStyle->hasVisibleStroke();
        } else {
            selectionStyle = style;
        }
    }

    if (textRenderer.frame() && textRenderer.frame()->view() && textRenderer.frame()->view()->paintBehavior() & PaintBehaviorRenderingSVGMask) {
        hasFill = true;
        hasVisibleStroke = false;
    }

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_paintingResource);

        GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity()) {
            stateSaver.save();
            paintInfo.context->concatCTM(fragmentTransform);
        }

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        unsigned decorations = style->textDecorationsInEffect();
        if (decorations & TextDecorationUnderline)
            paintDecoration(paintInfo.context, TextDecorationUnderline, fragment);
        if (decorations & TextDecorationOverline)
            paintDecoration(paintInfo.context, TextDecorationOverline, fragment);

        for (int i = 0; i < 3; i++) {
            switch (svgStyle->paintOrderType(i)) {
            case PT_FILL:
                // Fill text
                if (hasFill) {
                    paintText(paintInfo.context, style, selectionStyle, fragment,
                        ApplyToFillMode | ApplyToTextMode, hasSelection, paintSelectedTextOnly);
                }
                break;
            case PT_STROKE:
                // Stroke text
                if (hasVisibleStroke) {
                    paintText(paintInfo.context, style, selectionStyle, fragment,
                        ApplyToStrokeMode | ApplyToTextMode, hasSelection, paintSelectedTextOnly);
                }
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
            paintDecoration(paintInfo.context, TextDecorationLineThrough, fragment);
    }

    // finally, paint the outline if any
    if (style->hasOutline() && parentRenderer.isRenderInline())
        toRenderInline(parentRenderer).paintOutline(paintInfo, paintOffset);

    ASSERT(!m_paintingResource);
}

bool SVGInlineTextBox::acquirePaintingResource(GraphicsContext*& context, float scalingFactor,
    RenderObject* renderer, RenderStyle* style, RenderSVGResourceModeFlags resourceMode)
{
    // Callers must save the context state before calling when scalingFactor is not 1.
    ASSERT(scalingFactor);
    ASSERT(renderer);
    ASSERT(style);
    ASSERT(resourceMode != ApplyToDefaultMode);

    bool hasFallback = false;
    if (resourceMode & ApplyToFillMode)
        m_paintingResource = RenderSVGResource::fillPaintingResource(renderer, style, hasFallback);
    else if (resourceMode & ApplyToStrokeMode)
        m_paintingResource = RenderSVGResource::strokePaintingResource(renderer, style, hasFallback);
    else {
        // We're either called for stroking or filling.
        ASSERT_NOT_REACHED();
    }

    if (!m_paintingResource)
        return false;

    if (!m_paintingResource->applyResource(renderer, style, context, resourceMode)) {
        if (hasFallback) {
            m_paintingResource = RenderSVGResource::sharedSolidPaintingResource();
            m_paintingResource->applyResource(renderer, style, context, resourceMode);
        }
    }

    if (scalingFactor != 1 && resourceMode & ApplyToStrokeMode)
        context->setStrokeThickness(context->strokeThickness() * scalingFactor);

    return true;
}

void SVGInlineTextBox::releasePaintingResource(GraphicsContext*& context, const Path* path,
    RenderSVGResourceModeFlags resourceMode)
{
    ASSERT(m_paintingResource);

    m_paintingResource->postApplyResource(&parent()->renderer(), context, resourceMode, path, 0);
    m_paintingResource = 0;
}

bool SVGInlineTextBox::prepareGraphicsContextForTextPainting(GraphicsContext*& context,
    float scalingFactor, TextRun& textRun, RenderStyle* style, RenderSVGResourceModeFlags resourceMode)
{
    bool acquiredResource = acquirePaintingResource(context, scalingFactor, &parent()->renderer(), style, resourceMode);
    if (!acquiredResource)
        return false;

#if ENABLE(SVG_FONTS)
    // SVG Fonts need access to the painting resource used to draw the current text chunk.
    TextRun::RenderingContext* renderingContext = textRun.renderingContext();
    if (renderingContext)
        static_cast<SVGTextRunRenderingContext*>(renderingContext)->setActivePaintingResource(m_paintingResource);
#endif

    return true;
}

void SVGInlineTextBox::restoreGraphicsContextAfterTextPainting(GraphicsContext*& context,
    TextRun& textRun, RenderSVGResourceModeFlags resourceMode)
{
    releasePaintingResource(context, 0, resourceMode);

#if ENABLE(SVG_FONTS)
    TextRun::RenderingContext* renderingContext = textRun.renderingContext();
    if (renderingContext)
        static_cast<SVGTextRunRenderingContext*>(renderingContext)->setActivePaintingResource(0);
#endif
}

TextRun SVGInlineTextBox::constructTextRun(RenderStyle* style, const SVGTextFragment& fragment) const
{
    ASSERT(style);

    RenderText* text = &textRenderer();

    // FIXME(crbug.com/264211): This should not be necessary but can occur if we
    //                          layout during layout. Remove this when 264211 is fixed.
    RELEASE_ASSERT(!text->needsLayout());

    TextRun run(static_cast<const LChar*>(0) // characters, will be set below if non-zero.
                , 0 // length, will be set below if non-zero.
                , 0 // xPos, only relevant with allowTabs=true
                , 0 // padding, only relevant for justified text, not relevant for SVG
                , TextRun::AllowTrailingExpansion
                , direction()
                , dirOverride() || style->rtlOrdering() == VisualOrder /* directionalOverride */);

    if (fragment.length) {
        if (text->is8Bit())
            run.setText(text->characters8() + fragment.characterOffset, fragment.length);
        else
            run.setText(text->characters16() + fragment.characterOffset, fragment.length);
    }

    if (textRunNeedsRenderingContext(style->font()))
        run.setRenderingContext(SVGTextRunRenderingContext::create(text));

    run.disableRoundingHacks();

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(text->textLength() - fragment.characterOffset);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, int& startPosition, int& endPosition) const
{
    if (startPosition >= endPosition)
        return false;

    int offset = static_cast<int>(fragment.characterOffset) - start();
    int length = static_cast<int>(fragment.length);

    if (startPosition >= offset + length || endPosition <= offset)
        return false;

    if (startPosition < offset)
        startPosition = 0;
    else
        startPosition -= offset;

    if (endPosition > offset + length)
        endPosition = length;
    else {
        ASSERT(endPosition >= offset);
        endPosition -= offset;
    }

    ASSERT(startPosition < endPosition);
    return true;
}

static inline float positionOffsetForDecoration(TextDecoration decoration, const FontMetrics& fontMetrics, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera.
    if (decoration == TextDecorationUnderline)
        return fontMetrics.floatAscent() + thickness * 1.5f;
    if (decoration == TextDecorationOverline)
        return thickness;
    if (decoration == TextDecorationLineThrough)
        return fontMetrics.floatAscent() * 5 / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(TextDecoration, const Font& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera
    return font.fontDescription().computedSize() / 20.0f;
}

static inline RenderObject* findRenderObjectDefininingTextDecoration(InlineFlowBox* parentBox)
{
    // Lookup first render object in parent hierarchy which has text-decoration set.
    RenderObject* renderer = 0;
    while (parentBox) {
        renderer = &parentBox->renderer();

        if (renderer->style() && renderer->style()->textDecoration() != TextDecorationNone)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return renderer;
}

void SVGInlineTextBox::paintDecoration(GraphicsContext* context, TextDecoration decoration, const SVGTextFragment& fragment)
{
    if (textRenderer().style()->textDecorationsInEffect() == TextDecorationNone)
        return;

    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    RenderObject* decorationRenderer = findRenderObjectDefininingTextDecoration(parent());
    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    if (decorationStyle->visibility() == HIDDEN)
        return;

    const SVGRenderStyle* svgDecorationStyle = decorationStyle->svgStyle();
    ASSERT(svgDecorationStyle);

    for (int i = 0; i < 3; i++) {
        switch (svgDecorationStyle->paintOrderType(i)) {
        case PT_FILL:
            if (svgDecorationStyle->hasFill())
                paintDecorationWithStyle(context, decoration, fragment, decorationRenderer, ApplyToFillMode);
            break;
        case PT_STROKE:
            if (svgDecorationStyle->hasVisibleStroke())
                paintDecorationWithStyle(context, decoration, fragment, decorationRenderer, ApplyToStrokeMode);
            break;
        case PT_MARKERS:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void SVGInlineTextBox::paintDecorationWithStyle(GraphicsContext* context, TextDecoration decoration,
    const SVGTextFragment& fragment, RenderObject* decorationRenderer, RenderSVGResourceModeFlags resourceMode)
{
    ASSERT(!m_paintingResource);
    ASSERT(resourceMode != ApplyToDefaultMode);

    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    float scalingFactor = 1;
    Font scaledFont;
    RenderSVGInlineText::computeNewScaledFontForStyle(decorationRenderer, decorationStyle, scalingFactor, scaledFont);
    ASSERT(scalingFactor);

    // The initial y value refers to overline position.
    float thickness = thicknessForDecoration(decoration, scaledFont);

    if (fragment.width <= 0 && thickness <= 0)
        return;

    FloatPoint decorationOrigin(fragment.x, fragment.y);
    float width = fragment.width;
    const FontMetrics& scaledFontMetrics = scaledFont.fontMetrics();

    GraphicsContextStateSaver stateSaver(*context, false);
    if (scalingFactor != 1) {
        stateSaver.save();
        width *= scalingFactor;
        decorationOrigin.scale(scalingFactor, scalingFactor);
        context->scale(1 / scalingFactor, 1 / scalingFactor);
    }

    decorationOrigin.move(0, -scaledFontMetrics.floatAscent() + positionOffsetForDecoration(decoration, scaledFontMetrics, thickness));

    Path path;
    path.addRect(FloatRect(decorationOrigin, FloatSize(width, thickness)));

    // acquirePaintingResource also modifies state if the scalingFactor is non-identity.
    // Above we have saved the state for this case.
    if (acquirePaintingResource(context, scalingFactor, decorationRenderer, decorationStyle, resourceMode))
        releasePaintingResource(context, &path, resourceMode);
}

void SVGInlineTextBox::paintTextWithShadows(GraphicsContext* context, RenderStyle* style,
    TextRun& textRun, const SVGTextFragment& fragment, int startPosition, int endPosition,
    RenderSVGResourceModeFlags resourceMode)
{
    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textRenderer.scaledFont();
    const ShadowList* shadowList = style->textShadow();

    // Text shadows are disabled when printing. http://crbug.com/258321
    bool hasShadow = shadowList && !context->printing();

    FloatPoint textOrigin(fragment.x, fragment.y);
    FloatSize textSize(fragment.width, fragment.height);

    if (scalingFactor != 1) {
        textOrigin.scale(scalingFactor, scalingFactor);
        textSize.scale(scalingFactor);
        context->save();
        context->scale(1 / scalingFactor, 1 / scalingFactor);
    }

    if (hasShadow) {
        OwnPtr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
        for (size_t i = shadowList->shadows().size(); i--; ) {
            const ShadowData& shadow = shadowList->shadows()[i];
            FloatSize offset(shadow.x(), shadow.y());
            drawLooperBuilder->addShadow(offset, shadow.blur(), shadow.color(),
                DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowRespectsAlpha);
        }
        drawLooperBuilder->addUnmodifiedContent();
        context->setDrawLooper(drawLooperBuilder.release());
    }

    if (prepareGraphicsContextForTextPainting(context, scalingFactor, textRun, style, resourceMode)) {
        TextRunPaintInfo textRunPaintInfo(textRun);
        textRunPaintInfo.from = startPosition;
        textRunPaintInfo.to = endPosition;
        textRunPaintInfo.bounds = FloatRect(textOrigin, textSize);
        scaledFont.drawText(context, textRunPaintInfo, textOrigin);
        restoreGraphicsContextAfterTextPainting(context, textRun, resourceMode);
    }

    if (scalingFactor != 1)
        context->restore();
    else if (hasShadow)
        context->clearShadow();
}

void SVGInlineTextBox::paintText(GraphicsContext* context, RenderStyle* style,
    RenderStyle* selectionStyle, const SVGTextFragment& fragment,
    RenderSVGResourceModeFlags resourceMode, bool hasSelection, bool paintSelectedTextOnly)
{
    ASSERT(style);
    ASSERT(selectionStyle);

    int startPosition = 0;
    int endPosition = 0;
    if (hasSelection) {
        selectionStartEnd(startPosition, endPosition);
        hasSelection = mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
    }

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun = constructTextRun(style, fragment);
    if (!hasSelection || startPosition >= endPosition) {
        paintTextWithShadows(context, style, textRun, fragment, 0, fragment.length, resourceMode);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPosition > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, 0, startPosition, resourceMode);

    // Draw text using selection style from the start to the end position of the selection
    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsRepaintObject();
        SVGResourcesCache::clientStyleChanged(&parent()->renderer(), diff, selectionStyle);
    }

    paintTextWithShadows(context, selectionStyle, textRun, fragment, startPosition, endPosition, resourceMode);

    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsRepaintObject();
        SVGResourcesCache::clientStyleChanged(&parent()->renderer(), diff, selectionStyle);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < static_cast<int>(fragment.length) && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, endPosition, fragment.length, resourceMode);
}

void SVGInlineTextBox::paintDocumentMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, RenderStyle*, const Font&, bool)
{
    // SVG does not have support for generic document markers (e.g., spellchecking, etc).
}

void SVGInlineTextBox::paintTextMatchMarker(GraphicsContext* context, const FloatPoint&, DocumentMarker* marker, RenderStyle* style, const Font& font)
{
    // SVG is only interested in the TextMatch markers.
    if (marker->type() != DocumentMarker::TextMatch)
        return;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());

    FloatRect markerRect;
    AffineTransform fragmentTransform;
    for (InlineTextBox* box = textRenderer.firstTextBox(); box; box = box->nextTextBox()) {
        if (!box->isSVGInlineTextBox())
            continue;

        SVGInlineTextBox* textBox = toSVGInlineTextBox(box);

        int markerStartPosition = max<int>(marker->startOffset() - textBox->start(), 0);
        int markerEndPosition = min<int>(marker->endOffset() - textBox->start(), textBox->len());

        if (markerStartPosition >= markerEndPosition)
            continue;

        const Vector<SVGTextFragment>& fragments = textBox->textFragments();
        unsigned textFragmentsSize = fragments.size();
        for (unsigned i = 0; i < textFragmentsSize; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);

            int fragmentStartPosition = markerStartPosition;
            int fragmentEndPosition = markerEndPosition;
            if (!textBox->mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
                continue;

            FloatRect fragmentRect = textBox->selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
            fragment.buildFragmentTransform(fragmentTransform);

            // Draw the marker highlight.
            if (renderer().frame()->editor().markedTextMatchesAreHighlighted()) {
                Color color = marker->activeMatch() ?
                    RenderTheme::theme().platformActiveTextSearchHighlightColor() :
                    RenderTheme::theme().platformInactiveTextSearchHighlightColor();
                GraphicsContextStateSaver stateSaver(*context);
                if (!fragmentTransform.isIdentity())
                    context->concatCTM(fragmentTransform);
                context->setFillColor(color);
                context->fillRect(fragmentRect, color);
            }

            fragmentRect = fragmentTransform.mapRect(fragmentRect);
            markerRect.unite(fragmentRect);
        }
    }

    toRenderedDocumentMarker(marker)->setRenderedRect(textRenderer.localToAbsoluteQuad(markerRect).enclosingBoundingBox());
}

FloatRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->textRenderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    float baseline = textRenderer.scaledFont().fontMetrics().floatAscent() / scalingFactor;

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);
        FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
        fragment.buildFragmentTransform(fragmentTransform);
        fragmentRect = fragmentTransform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }

    return textRect;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit, LayoutUnit)
{
    // FIXME: integrate with InlineTextBox::nodeAtPoint better.
    ASSERT(!isLineBreak());

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, request, renderer().style()->pointerEvents());
    bool isVisible = renderer().style()->visibility() == VISIBLE;
    if (isVisible || !hitRules.requireVisible) {
        if (hitRules.canHitBoundingBox
            || (hitRules.canHitStroke && (renderer().style()->svgStyle()->hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (renderer().style()->svgStyle()->hasFill() || !hitRules.requireFill))) {
            FloatPoint boxOrigin(x(), y());
            boxOrigin.moveBy(accumulatedOffset);
            FloatRect rect(boxOrigin, size());
            if (locationInContainer.intersects(rect)) {
                renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                if (!result.addNodeToRectBasedTestResult(renderer().node(), request, locationInContainer, rect))
                    return true;
             }
        }
    }
    return false;
}

} // namespace WebCore
