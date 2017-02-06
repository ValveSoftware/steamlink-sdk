/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "core/layout/svg/SVGTextLayoutEngine.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutSVGTextPath.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGTextChunkBuilder.h"
#include "core/layout/svg/SVGTextLayoutEngineBaseline.h"
#include "core/layout/svg/SVGTextLayoutEngineSpacing.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGTextContentElement.h"

namespace blink {

SVGTextLayoutEngine::SVGTextLayoutEngine(const Vector<LayoutSVGInlineText*>& descendantTextNodes)
    : m_descendantTextNodes(descendantTextNodes)
    , m_currentLogicalTextNodeIndex(0)
    , m_logicalCharacterOffset(0)
    , m_logicalMetricsListOffset(0)
    , m_isVerticalText(false)
    , m_inPathLayout(false)
    , m_textLengthSpacingInEffect(false)
    , m_textPath(nullptr)
    , m_textPathCurrentOffset(0)
    , m_textPathDisplacement(0)
    , m_textPathSpacing(0)
    , m_textPathScaling(1)
{
    ASSERT(!m_descendantTextNodes.isEmpty());
}

SVGTextLayoutEngine::~SVGTextLayoutEngine() = default;

bool SVGTextLayoutEngine::setCurrentTextPosition(const SVGCharacterData& data)
{
    bool hasX = data.hasX();
    if (hasX)
        m_textPosition.setX(data.x);

    bool hasY = data.hasY();
    if (hasY)
        m_textPosition.setY(data.y);

    // If there's an absolute x/y position available, it marks the beginning of
    // a new position along the path.
    if (m_inPathLayout) {
        // TODO(fs): If a new chunk (== absolute position) is defined while in
        // path layout mode, alignment should be based on that chunk and not
        // the path as a whole. (Re: the addition of m_textPathStartOffset
        // below.)
        if (m_isVerticalText) {
            if (hasY)
                m_textPathCurrentOffset = data.y + m_textPathStartOffset;
        } else {
            if (hasX)
                m_textPathCurrentOffset = data.x + m_textPathStartOffset;
        }
    }
    return hasX || hasY;
}

void SVGTextLayoutEngine::advanceCurrentTextPosition(float glyphAdvance)
{
    // TODO(fs): m_textPathCurrentOffset should preferably also be updated
    // here, but that requires a bit more untangling yet.
    if (m_isVerticalText)
        m_textPosition.setY(m_textPosition.y() + glyphAdvance);
    else
        m_textPosition.setX(m_textPosition.x() + glyphAdvance);
}

bool SVGTextLayoutEngine::applyRelativePositionAdjustmentsIfNeeded(const SVGCharacterData& data)
{
    FloatPoint delta;
    bool hasDx = data.hasDx();
    if (hasDx)
        delta.setX(data.dx);

    bool hasDy = data.hasDy();
    if (hasDy)
        delta.setY(data.dy);

    // Apply dx/dy value adjustments to current text position, if needed.
    m_textPosition.moveBy(delta);

    if (m_inPathLayout) {
        if (m_isVerticalText)
            delta = delta.transposedPoint();

        m_textPathCurrentOffset += delta.x();
        m_textPathDisplacement += delta.y();
    }
    return hasDx || hasDy;
}

void SVGTextLayoutEngine::computeCurrentFragmentMetrics(SVGInlineTextBox* textBox)
{
    LineLayoutSVGInlineText textLineLayout = LineLayoutSVGInlineText(textBox->getLineLayoutItem());
    TextRun run = textBox->constructTextRun(textLineLayout.styleRef(), m_currentTextFragment);

    float scalingFactor = textLineLayout.scalingFactor();
    ASSERT(scalingFactor);
    const Font& scaledFont = textLineLayout.scaledFont();
    FloatRect glyphOverflowBounds;

    float width = scaledFont.width(run, nullptr, &glyphOverflowBounds);
    float ascent = scaledFont.getFontMetrics().floatAscent();
    float descent = scaledFont.getFontMetrics().floatDescent();
    m_currentTextFragment.glyphOverflow.setFromBounds(glyphOverflowBounds, ascent, descent, width);
    m_currentTextFragment.glyphOverflow.top /= scalingFactor;
    m_currentTextFragment.glyphOverflow.left /= scalingFactor;
    m_currentTextFragment.glyphOverflow.right /= scalingFactor;
    m_currentTextFragment.glyphOverflow.bottom /= scalingFactor;

    float height = scaledFont.getFontMetrics().floatHeight();
    m_currentTextFragment.height = height / scalingFactor;
    m_currentTextFragment.width = width / scalingFactor;
}

void SVGTextLayoutEngine::recordTextFragment(SVGInlineTextBox* textBox)
{
    ASSERT(!m_currentTextFragment.length);

    // Figure out length of fragment.
    m_currentTextFragment.length = m_visualMetricsIterator.characterOffset() - m_currentTextFragment.characterOffset;

    // Figure out fragment metrics.
    computeCurrentFragmentMetrics(textBox);

    textBox->textFragments().append(m_currentTextFragment);
    m_currentTextFragment = SVGTextFragment();
}

void SVGTextLayoutEngine::beginTextPathLayout(SVGInlineFlowBox* flowBox)
{
    // Build text chunks for all <textPath> children, using the line layout algorithm.
    // This is needeed as text-anchor is just an additional startOffset for text paths.
    SVGTextLayoutEngine lineLayout(m_descendantTextNodes);
    lineLayout.m_textLengthSpacingInEffect = m_textLengthSpacingInEffect;
    lineLayout.layoutCharactersInTextBoxes(flowBox);

    m_inPathLayout = true;
    LineLayoutSVGTextPath textPath = LineLayoutSVGTextPath(flowBox->getLineLayoutItem());

    m_textPath = textPath.layoutPath();
    if (!m_textPath)
        return;
    m_textPathStartOffset = textPath.calculateStartOffset(m_textPath->length());

    SVGTextPathChunkBuilder textPathChunkLayoutBuilder;
    textPathChunkLayoutBuilder.processTextChunks(lineLayout.m_lineLayoutBoxes);

    m_textPathStartOffset += textPathChunkLayoutBuilder.totalTextAnchorShift();
    m_textPathCurrentOffset = m_textPathStartOffset;

    // Eventually handle textLength adjustments.
    SVGLengthAdjustType lengthAdjust = SVGLengthAdjustUnknown;
    float desiredTextLength = 0;

    if (SVGTextContentElement* textContentElement = SVGTextContentElement::elementFromLineLayoutItem(textPath)) {
        SVGLengthContext lengthContext(textContentElement);
        lengthAdjust = textContentElement->lengthAdjust()->currentValue()->enumValue();
        if (textContentElement->textLengthIsSpecifiedByUser())
            desiredTextLength = textContentElement->textLength()->currentValue()->value(lengthContext);
        else
            desiredTextLength = 0;
    }

    if (!desiredTextLength)
        return;

    float totalLength = textPathChunkLayoutBuilder.totalLength();
    if (lengthAdjust == SVGLengthAdjustSpacing)
        m_textPathSpacing = (desiredTextLength - totalLength) / textPathChunkLayoutBuilder.totalCharacters();
    else
        m_textPathScaling = desiredTextLength / totalLength;
}

void SVGTextLayoutEngine::endTextPathLayout()
{
    m_inPathLayout = false;
    m_textPath = nullptr;
    m_textPathStartOffset = 0;
    m_textPathCurrentOffset = 0;
    m_textPathSpacing = 0;
    m_textPathScaling = 1;
}

void SVGTextLayoutEngine::layoutInlineTextBox(SVGInlineTextBox* textBox)
{
    ASSERT(textBox);

    LineLayoutSVGInlineText textLineLayout = LineLayoutSVGInlineText(textBox->getLineLayoutItem());
    ASSERT(textLineLayout.parent());
    ASSERT(textLineLayout.parent().node());
    ASSERT(textLineLayout.parent().node()->isSVGElement());

    const ComputedStyle& style = textLineLayout.styleRef();

    textBox->clearTextFragments();
    m_isVerticalText = !style.isHorizontalWritingMode();
    layoutTextOnLineOrPath(textBox, textLineLayout, style);

    if (m_inPathLayout)
        return;

    m_lineLayoutBoxes.append(textBox);
}

static bool definesTextLengthWithSpacing(const InlineFlowBox* start)
{
    SVGTextContentElement* textContentElement = SVGTextContentElement::elementFromLineLayoutItem(start->getLineLayoutItem());
    return textContentElement
        && textContentElement->lengthAdjust()->currentValue()->enumValue() == SVGLengthAdjustSpacing
        && textContentElement->textLengthIsSpecifiedByUser();
}

void SVGTextLayoutEngine::layoutCharactersInTextBoxes(InlineFlowBox* start)
{
    bool textLengthSpacingInEffect = m_textLengthSpacingInEffect || definesTextLengthWithSpacing(start);
    TemporaryChange<bool> textLengthSpacingScope(m_textLengthSpacingInEffect, textLengthSpacingInEffect);

    for (InlineBox* child = start->firstChild(); child; child = child->nextOnLine()) {
        if (child->isSVGInlineTextBox()) {
            ASSERT(child->getLineLayoutItem().isSVGInlineText());
            layoutInlineTextBox(toSVGInlineTextBox(child));
        } else {
            // Skip generated content.
            Node* node = child->getLineLayoutItem().node();
            if (!node)
                continue;

            SVGInlineFlowBox* flowBox = toSVGInlineFlowBox(child);
            bool isTextPath = isSVGTextPathElement(*node);
            if (isTextPath)
                beginTextPathLayout(flowBox);

            layoutCharactersInTextBoxes(flowBox);

            if (isTextPath)
                endTextPathLayout();
        }
    }
}

void SVGTextLayoutEngine::finishLayout()
{
    m_visualMetricsIterator = SVGInlineTextMetricsIterator();

    // After all text fragments are stored in their correpsonding SVGInlineTextBoxes, we can layout individual text chunks.
    // Chunk layouting is only performed for line layout boxes, not for path layout, where it has already been done.
    SVGTextChunkBuilder chunkLayoutBuilder;
    chunkLayoutBuilder.processTextChunks(m_lineLayoutBoxes);

    m_lineLayoutBoxes.clear();
}

const LayoutSVGInlineText* SVGTextLayoutEngine::nextLogicalTextNode()
{
    ASSERT(m_currentLogicalTextNodeIndex < m_descendantTextNodes.size());
    ++m_currentLogicalTextNodeIndex;
    if (m_currentLogicalTextNodeIndex == m_descendantTextNodes.size())
        return nullptr;

    m_logicalMetricsListOffset = 0;
    m_logicalCharacterOffset = 0;
    return m_descendantTextNodes[m_currentLogicalTextNodeIndex];
}

const LayoutSVGInlineText* SVGTextLayoutEngine::currentLogicalCharacterMetrics(SVGTextMetrics& logicalMetrics)
{
    // If we've consumed all text nodes, there can be no more metrics.
    if (m_currentLogicalTextNodeIndex == m_descendantTextNodes.size())
        return nullptr;

    const LayoutSVGInlineText* logicalTextNode = m_descendantTextNodes[m_currentLogicalTextNodeIndex];
    const Vector<SVGTextMetrics>* metricsList = &logicalTextNode->metricsList();
    unsigned metricsListSize = metricsList->size();
    ASSERT(m_logicalMetricsListOffset <= metricsListSize);

    // Find the next non-collapsed text metrics cell.
    while (true) {
        // If we run out of metrics, move to the next set of non-empty layout
        // attributes.
        if (m_logicalMetricsListOffset == metricsListSize) {
            logicalTextNode = nextLogicalTextNode();
            if (!logicalTextNode)
                return nullptr;
            metricsList = &logicalTextNode->metricsList();
            metricsListSize = metricsList->size();
            // Return to the while so that we check if the new metrics list is
            // non-empty before using it.
            continue;
        }

        ASSERT(metricsListSize);
        logicalMetrics = metricsList->at(m_logicalMetricsListOffset);
        // Stop if we found the next valid logical text metrics object.
        if (!logicalMetrics.isEmpty())
            break;

        advanceToNextLogicalCharacter(logicalMetrics);
    }

    return logicalTextNode;
}

void SVGTextLayoutEngine::advanceToNextLogicalCharacter(const SVGTextMetrics& logicalMetrics)
{
    ++m_logicalMetricsListOffset;
    m_logicalCharacterOffset += logicalMetrics.length();
}

void SVGTextLayoutEngine::layoutTextOnLineOrPath(SVGInlineTextBox* textBox, LineLayoutSVGInlineText textLineLayout, const ComputedStyle& style)
{
    if (m_inPathLayout && !m_textPath)
        return;

    // Find the start of the current text box in the metrics list.
    m_visualMetricsIterator.advanceToTextStart(textLineLayout, textBox->start());

    const Font& font = style.font();

    SVGTextLayoutEngineSpacing spacingLayout(font, style.effectiveZoom());
    SVGTextLayoutEngineBaseline baselineLayout(font, style.effectiveZoom());

    bool didStartTextFragment = false;
    bool applySpacingToNextCharacter = false;
    bool needsFragmentPerGlyph = m_isVerticalText || m_inPathLayout || m_textLengthSpacingInEffect;

    float lastAngle = 0;
    float baselineShiftValue = baselineLayout.calculateBaselineShift(style);
    baselineShiftValue -= baselineLayout.calculateAlignmentBaselineShift(m_isVerticalText, textLineLayout);
    FloatPoint baselineShift;
    if (m_isVerticalText)
        baselineShift.setX(baselineShiftValue);
    else
        baselineShift.setY(-baselineShiftValue);

    // Main layout algorithm.
    const unsigned boxEndOffset = textBox->start() + textBox->len();
    while (!m_visualMetricsIterator.isAtEnd() && m_visualMetricsIterator.characterOffset() < boxEndOffset) {
        const SVGTextMetrics& visualMetrics = m_visualMetricsIterator.metrics();
        if (visualMetrics.isEmpty()) {
            m_visualMetricsIterator.next();
            continue;
        }

        SVGTextMetrics logicalMetrics(SVGTextMetrics::SkippedSpaceMetrics);
        const LayoutSVGInlineText* logicalTextNode = currentLogicalCharacterMetrics(logicalMetrics);
        if (!logicalTextNode)
            break;

        const SVGCharacterData data = logicalTextNode->characterDataMap().get(m_logicalCharacterOffset + 1);

        // TODO(fs): Use the return value to eliminate the additional
        // hash-lookup below when determining if this text box should be tagged
        // as starting a new text chunk.
        setCurrentTextPosition(data);

        // When we've advanced to the box start offset, determine using the original x/y values,
        // whether this character starts a new text chunk, before doing any further processing.
        if (m_visualMetricsIterator.characterOffset() == textBox->start())
            textBox->setStartsNewTextChunk(logicalTextNode->characterStartsNewTextChunk(m_logicalCharacterOffset));

        bool hasRelativePosition = applyRelativePositionAdjustmentsIfNeeded(data);

        // Determine the orientation of the current glyph.
        // Font::width() calculates the resolved FontOrientation for each character,
        // but that value is not exposed today to avoid the API complexity.
        UChar32 currentCharacter = textLineLayout.codepointAt(m_visualMetricsIterator.characterOffset());
        FontOrientation fontOrientation = font.getFontDescription().orientation();
        fontOrientation = adjustOrientationForCharacterInMixedVertical(fontOrientation, currentCharacter);

        // Calculate glyph advance.
        // The shaping engine takes care of x/y orientation shifts for different fontOrientation values.
        float glyphAdvance = visualMetrics.advance(fontOrientation);

        // Calculate CSS 'letter-spacing' and 'word-spacing' for the character, if needed.
        float spacing = spacingLayout.calculateCSSSpacing(currentCharacter);

        FloatPoint textPathShift;
        float angle = 0;
        FloatPoint position;
        if (m_inPathLayout) {
            float scaledGlyphAdvance = glyphAdvance * m_textPathScaling;
            // Setup translations that move to the glyph midpoint.
            textPathShift.set(-scaledGlyphAdvance / 2, m_textPathDisplacement);
            if (m_isVerticalText)
                textPathShift = textPathShift.transposedPoint();
            textPathShift += baselineShift;

            // Calculate current offset along path.
            float textPathOffset = m_textPathCurrentOffset + scaledGlyphAdvance / 2;

            // Move to next character.
            m_textPathCurrentOffset += scaledGlyphAdvance + m_textPathSpacing + spacing * m_textPathScaling;

            PathPositionMapper::PositionType positionType = m_textPath->pointAndNormalAtLength(textPathOffset, position, angle);

            // Skip character, if we're before the path.
            if (positionType == PathPositionMapper::BeforePath) {
                advanceToNextLogicalCharacter(logicalMetrics);
                m_visualMetricsIterator.next();
                continue;
            }

            // Stop processing if the next character lies behind the path.
            if (positionType == PathPositionMapper::AfterPath)
                break;

            m_textPosition = position;

            // For vertical text on path, the actual angle has to be rotated 90 degrees anti-clockwise, not the orientation angle!
            if (m_isVerticalText)
                angle -= 90;
        } else {
            position = m_textPosition;
            position += baselineShift;
        }

        if (data.hasRotate())
            angle += data.rotate;

        // Determine whether we have to start a new fragment.
        bool shouldStartNewFragment = needsFragmentPerGlyph || hasRelativePosition
            || angle || angle != lastAngle || applySpacingToNextCharacter;

        // If we already started a fragment, close it now.
        if (didStartTextFragment && shouldStartNewFragment) {
            applySpacingToNextCharacter = false;
            recordTextFragment(textBox);
        }

        // Eventually start a new fragment, if not yet done.
        if (!didStartTextFragment || shouldStartNewFragment) {
            ASSERT(!m_currentTextFragment.characterOffset);
            ASSERT(!m_currentTextFragment.length);

            didStartTextFragment = true;
            m_currentTextFragment.characterOffset = m_visualMetricsIterator.characterOffset();
            m_currentTextFragment.metricsListOffset = m_visualMetricsIterator.metricsListOffset();
            m_currentTextFragment.x = position.x();
            m_currentTextFragment.y = position.y();

            // Build fragment transformation.
            if (angle)
                m_currentTextFragment.transform.rotate(angle);

            if (textPathShift.x() || textPathShift.y())
                m_currentTextFragment.transform.translate(textPathShift.x(), textPathShift.y());

            // For vertical text, always rotate by 90 degrees regardless of fontOrientation.
            // The shaping engine takes care of the necessary orientation.
            if (m_isVerticalText)
                m_currentTextFragment.transform.rotate(90);

            m_currentTextFragment.isVertical = m_isVerticalText;
            m_currentTextFragment.isTextOnPath = m_inPathLayout && m_textPathScaling != 1;
            if (m_currentTextFragment.isTextOnPath)
                m_currentTextFragment.lengthAdjustScale = m_textPathScaling;
        }

        // Advance current text position after processing of the current character finished.
        advanceCurrentTextPosition(glyphAdvance + spacing);

        // Apply CSS 'letter-spacing' and 'word-spacing' to the next character, if needed.
        if (!m_inPathLayout && spacing)
            applySpacingToNextCharacter = true;

        advanceToNextLogicalCharacter(logicalMetrics);
        m_visualMetricsIterator.next();
        lastAngle = angle;
    }

    if (!didStartTextFragment)
        return;

    // Close last open fragment, if needed.
    recordTextFragment(textBox);
}

} // namespace blink
