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

#include "core/layout/svg/SVGTextQuery.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGTextFragment.h"
#include "core/layout/svg/SVGTextMetrics.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "platform/FloatConversion.h"
#include "wtf/MathExtras.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

// Base structure for callback user data
struct QueryData {
    QueryData()
        : isVerticalText(false)
        , currentOffset(0)
        , textLineLayout(nullptr)
        , textBox(nullptr)
    {
    }

    bool isVerticalText;
    unsigned currentOffset;
    LineLayoutSVGInlineText textLineLayout;
    const SVGInlineTextBox* textBox;
};

static inline InlineFlowBox* flowBoxForLayoutObject(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return nullptr;

    if (layoutObject->isLayoutBlock()) {
        // If we're given a block element, it has to be a LayoutSVGText.
        ASSERT(layoutObject->isSVGText());
        LayoutBlockFlow* layoutBlockFlow = toLayoutBlockFlow(layoutObject);

        // LayoutSVGText only ever contains a single line box.
        InlineFlowBox* flowBox = layoutBlockFlow->firstLineBox();
        ASSERT(flowBox == layoutBlockFlow->lastLineBox());
        return flowBox;
    }

    if (layoutObject->isLayoutInline()) {
        // We're given a LayoutSVGInline or objects that derive from it (LayoutSVGTSpan / LayoutSVGTextPath)
        LayoutInline* layoutInline = toLayoutInline(layoutObject);

        // LayoutSVGInline only ever contains a single line box.
        InlineFlowBox* flowBox = layoutInline->firstLineBox();
        ASSERT(flowBox == layoutInline->lastLineBox());
        return flowBox;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static void collectTextBoxesInFlowBox(InlineFlowBox* flowBox, Vector<SVGInlineTextBox*>& textBoxes)
{
    if (!flowBox)
        return;

    for (InlineBox* child = flowBox->firstChild(); child; child = child->nextOnLine()) {
        if (child->isInlineFlowBox()) {
            // Skip generated content.
            if (!child->getLineLayoutItem().node())
                continue;

            collectTextBoxesInFlowBox(toInlineFlowBox(child), textBoxes);
            continue;
        }

        if (child->isSVGInlineTextBox())
            textBoxes.append(toSVGInlineTextBox(child));
    }
}

typedef bool ProcessTextFragmentCallback(QueryData*, const SVGTextFragment&);

static bool queryTextBox(QueryData* queryData, const SVGInlineTextBox* textBox, ProcessTextFragmentCallback fragmentCallback)
{
    queryData->textBox = textBox;
    queryData->textLineLayout = LineLayoutSVGInlineText(textBox->getLineLayoutItem());

    queryData->isVerticalText = !queryData->textLineLayout.style()->isHorizontalWritingMode();

    // Loop over all text fragments in this text box, firing a callback for each.
    for (const SVGTextFragment& fragment : textBox->textFragments()) {
        if (fragmentCallback(queryData, fragment))
            return true;
    }
    return false;
}

// Execute a query in "spatial" order starting at |queryRoot|. This means
// walking the lines boxes in the order they would get painted.
static void spatialQuery(LayoutObject* queryRoot, QueryData* queryData, ProcessTextFragmentCallback fragmentCallback)
{
    Vector<SVGInlineTextBox*> textBoxes;
    collectTextBoxesInFlowBox(flowBoxForLayoutObject(queryRoot), textBoxes);

    // Loop over all text boxes
    for (const SVGInlineTextBox* textBox : textBoxes) {
        if (queryTextBox(queryData, textBox, fragmentCallback))
            return;
    }
}

static void collectTextBoxesInLogicalOrder(LineLayoutSVGInlineText textLineLayout, Vector<SVGInlineTextBox*>& textBoxes)
{
    textBoxes.shrink(0);
    for (InlineTextBox* textBox = textLineLayout.firstTextBox(); textBox; textBox = textBox->nextTextBox())
        textBoxes.append(toSVGInlineTextBox(textBox));
    std::sort(textBoxes.begin(), textBoxes.end(), InlineTextBox::compareByStart);
}

// Execute a query in "logical" order starting at |queryRoot|. This means
// walking the lines boxes for each layout object in layout tree (pre)order.
static void logicalQuery(LayoutObject* queryRoot, QueryData* queryData, ProcessTextFragmentCallback fragmentCallback)
{
    if (!queryRoot)
        return;

    // Walk the layout tree in pre-order, starting at the specified root, and
    // run the query for each text node.
    Vector<SVGInlineTextBox*> textBoxes;
    for (LayoutObject* layoutObject = queryRoot->slowFirstChild(); layoutObject; layoutObject = layoutObject->nextInPreOrder(queryRoot)) {
        if (!layoutObject->isSVGInlineText())
            continue;

        LineLayoutSVGInlineText textLineLayout = LineLayoutSVGInlineText(toLayoutSVGInlineText(layoutObject));
        ASSERT(textLineLayout.style());

        // TODO(fs): Allow filtering the search earlier, since we should be
        // able to trivially reject (prune) at least some of the queries.
        collectTextBoxesInLogicalOrder(textLineLayout, textBoxes);

        for (const SVGInlineTextBox* textBox : textBoxes) {
            if (queryTextBox(queryData, textBox, fragmentCallback))
                return;
            queryData->currentOffset += textBox->len();
        }
    }
}

static bool mapStartEndPositionsIntoFragmentCoordinates(const QueryData* queryData, const SVGTextFragment& fragment, int& startPosition, int& endPosition)
{
    unsigned boxStart = queryData->currentOffset;

    // Make <startPosition, endPosition> offsets relative to the current text box.
    startPosition -= boxStart;
    endPosition -= boxStart;

    // Reuse the same logic used for text selection & painting, to map our
    // query start/length into start/endPositions of the current text fragment.
    return queryData->textBox->mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
}

// numberOfCharacters() implementation
static bool numberOfCharactersCallback(QueryData*, const SVGTextFragment&)
{
    // no-op
    return false;
}

unsigned SVGTextQuery::numberOfCharacters() const
{
    QueryData data;
    logicalQuery(m_queryRootLayoutObject, &data, numberOfCharactersCallback);
    return data.currentOffset;
}

// textLength() implementation
struct TextLengthData : QueryData {
    TextLengthData()
        : textLength(0)
    {
    }

    float textLength;
};

static bool textLengthCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    TextLengthData* data = static_cast<TextLengthData*>(queryData);
    data->textLength += queryData->isVerticalText ? fragment.height : fragment.width;
    return false;
}

float SVGTextQuery::textLength() const
{
    TextLengthData data;
    logicalQuery(m_queryRootLayoutObject, &data, textLengthCallback);
    return data.textLength;
}

using MetricsList = Vector<SVGTextMetrics>;

MetricsList::const_iterator findMetricsForCharacter(const MetricsList& metricsList, const SVGTextFragment& fragment, unsigned startInFragment)
{
    // Find the text metrics cell that starts at or contains the character at |startInFragment|.
    MetricsList::const_iterator metrics = metricsList.begin() + fragment.metricsListOffset;
    unsigned fragmentOffset = 0;
    while (fragmentOffset < fragment.length) {
        fragmentOffset += metrics->length();
        if (startInFragment < fragmentOffset)
            break;
        ++metrics;
    }
    ASSERT(metrics <= metricsList.end());
    return metrics;
}

static float calculateGlyphRange(const QueryData* queryData, const SVGTextFragment& fragment, unsigned start, unsigned end)
{
    const MetricsList& metricsList = queryData->textLineLayout.metricsList();
    auto metrics = findMetricsForCharacter(metricsList, fragment, start);
    auto endMetrics = findMetricsForCharacter(metricsList, fragment, end);
    float glyphRange = 0;
    for (; metrics != endMetrics; ++metrics)
        glyphRange += queryData->isVerticalText ? metrics->height() : metrics->width();
    return glyphRange;
}

// subStringLength() implementation
struct SubStringLengthData : QueryData {
    SubStringLengthData(unsigned queryStartPosition, unsigned queryLength)
        : startPosition(queryStartPosition)
        , length(queryLength)
        , subStringLength(0)
    {
    }

    unsigned startPosition;
    unsigned length;

    float subStringLength;
};

static bool subStringLengthCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    SubStringLengthData* data = static_cast<SubStringLengthData*>(queryData);

    int startPosition = data->startPosition;
    int endPosition = startPosition + data->length;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->subStringLength += calculateGlyphRange(queryData, fragment, startPosition, endPosition);
    return false;
}

float SVGTextQuery::subStringLength(unsigned startPosition, unsigned length) const
{
    SubStringLengthData data(startPosition, length);
    logicalQuery(m_queryRootLayoutObject, &data, subStringLengthCallback);
    return data.subStringLength;
}

// startPositionOfCharacter() implementation
struct StartPositionOfCharacterData : QueryData {
    StartPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint startPosition;
};

static FloatPoint logicalGlyphPositionToPhysical(const QueryData* queryData, const SVGTextFragment& fragment, float logicalGlyphOffset)
{
    float physicalGlyphOffset = logicalGlyphOffset;
    if (!queryData->textBox->isLeftToRightDirection()) {
        float fragmentExtent = queryData->isVerticalText ? fragment.height : fragment.width;
        physicalGlyphOffset = fragmentExtent - logicalGlyphOffset;
    }

    FloatPoint glyphPosition(fragment.x, fragment.y);
    if (queryData->isVerticalText)
        glyphPosition.move(0, physicalGlyphOffset);
    else
        glyphPosition.move(physicalGlyphOffset, 0);

    return glyphPosition;
}

static FloatPoint calculateGlyphPosition(const QueryData* queryData, const SVGTextFragment& fragment, unsigned offsetInFragment)
{
    float glyphOffsetInDirection = calculateGlyphRange(queryData, fragment, 0, offsetInFragment);
    FloatPoint glyphPosition = logicalGlyphPositionToPhysical(queryData, fragment, glyphOffsetInDirection);
    if (fragment.isTransformed()) {
        AffineTransform fragmentTransform = fragment.buildFragmentTransform(SVGTextFragment::TransformIgnoringTextLength);
        glyphPosition = fragmentTransform.mapPoint(glyphPosition);
    }
    return glyphPosition;
}

static bool startPositionOfCharacterCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    StartPositionOfCharacterData* data = static_cast<StartPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->startPosition = calculateGlyphPosition(queryData, fragment, startPosition);
    return true;
}

FloatPoint SVGTextQuery::startPositionOfCharacter(unsigned position) const
{
    StartPositionOfCharacterData data(position);
    logicalQuery(m_queryRootLayoutObject, &data, startPositionOfCharacterCallback);
    return data.startPosition;
}

// endPositionOfCharacter() implementation
struct EndPositionOfCharacterData : QueryData {
    EndPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint endPosition;
};

static bool endPositionOfCharacterCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    EndPositionOfCharacterData* data = static_cast<EndPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->endPosition = calculateGlyphPosition(queryData, fragment, endPosition);
    return true;
}

FloatPoint SVGTextQuery::endPositionOfCharacter(unsigned position) const
{
    EndPositionOfCharacterData data(position);
    logicalQuery(m_queryRootLayoutObject, &data, endPositionOfCharacterCallback);
    return data.endPosition;
}

// rotationOfCharacter() implementation
struct RotationOfCharacterData : QueryData {
    RotationOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
        , rotation(0)
    {
    }

    unsigned position;
    float rotation;
};

static bool rotationOfCharacterCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    RotationOfCharacterData* data = static_cast<RotationOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    if (!fragment.isTransformed()) {
        data->rotation = 0;
    } else {
        AffineTransform fragmentTransform = fragment.buildFragmentTransform(SVGTextFragment::TransformIgnoringTextLength);
        fragmentTransform.scale(1 / fragmentTransform.xScale(), 1 / fragmentTransform.yScale());
        data->rotation = narrowPrecisionToFloat(rad2deg(atan2(fragmentTransform.b(), fragmentTransform.a())));
    }
    return true;
}

float SVGTextQuery::rotationOfCharacter(unsigned position) const
{
    RotationOfCharacterData data(position);
    logicalQuery(m_queryRootLayoutObject, &data, rotationOfCharacterCallback);
    return data.rotation;
}

// extentOfCharacter() implementation
struct ExtentOfCharacterData : QueryData {
    ExtentOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatRect extent;
};

static FloatRect physicalGlyphExtents(const QueryData* queryData, const SVGTextMetrics& metrics, const FloatPoint& glyphPosition)
{
    // TODO(fs): Negative glyph extents seems kind of weird to have, but
    // presently it can occur in some cases (like Arabic.)
    FloatRect glyphExtents(
        glyphPosition,
        FloatSize(std::max<float>(metrics.width(), 0), std::max<float>(metrics.height(), 0)));

    // If RTL, adjust the starting point to align with the LHS of the glyph bounding box.
    if (!queryData->textBox->isLeftToRightDirection()) {
        if (queryData->isVerticalText)
            glyphExtents.move(0, -glyphExtents.height());
        else
            glyphExtents.move(-glyphExtents.width(), 0);
    }
    return glyphExtents;
}

static inline FloatRect calculateGlyphBoundaries(const QueryData* queryData, const SVGTextFragment& fragment, int startPosition)
{
    const float scalingFactor = queryData->textLineLayout.scalingFactor();
    ASSERT(scalingFactor);
    const float baseline = queryData->textLineLayout.scaledFont().getFontMetrics().floatAscent() / scalingFactor;

    float glyphOffsetInDirection = calculateGlyphRange(queryData, fragment, 0, startPosition);
    FloatPoint glyphPosition = logicalGlyphPositionToPhysical(queryData, fragment, glyphOffsetInDirection);
    glyphPosition.move(0, -baseline);

    // Use the SVGTextMetrics computed by SVGTextMetricsBuilder.
    const MetricsList& metricsList = queryData->textLineLayout.metricsList();
    auto metrics = findMetricsForCharacter(metricsList, fragment, startPosition);

    FloatRect extent = physicalGlyphExtents(queryData, *metrics, glyphPosition);
    if (fragment.isTransformed()) {
        AffineTransform fragmentTransform = fragment.buildFragmentTransform(SVGTextFragment::TransformIgnoringTextLength);
        extent = fragmentTransform.mapRect(extent);
    }
    return extent;
}

static bool extentOfCharacterCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    ExtentOfCharacterData* data = static_cast<ExtentOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->extent = calculateGlyphBoundaries(queryData, fragment, startPosition);
    return true;
}

FloatRect SVGTextQuery::extentOfCharacter(unsigned position) const
{
    ExtentOfCharacterData data(position);
    logicalQuery(m_queryRootLayoutObject, &data, extentOfCharacterCallback);
    return data.extent;
}

// characterNumberAtPosition() implementation
struct CharacterNumberAtPositionData : QueryData {
    CharacterNumberAtPositionData(const FloatPoint& queryPosition)
        : position(queryPosition)
        , hitLayoutItem(nullptr)
        , offsetInTextNode(0)
    {
    }

    int characterNumberWithin(const LayoutObject* queryRoot) const;

    FloatPoint position;
    LineLayoutItem hitLayoutItem;
    int offsetInTextNode;
};

int CharacterNumberAtPositionData::characterNumberWithin(const LayoutObject* queryRoot) const
{
    // http://www.w3.org/TR/SVG/single-page.html#text-__svg__SVGTextContentElement__getCharNumAtPosition
    // "If no such character exists, a value of -1 is returned."
    if (!hitLayoutItem)
        return -1;
    ASSERT(queryRoot);
    int characterNumber = offsetInTextNode;

    // Accumulate the lengths of all the text nodes preceding the target layout
    // object within the queried root, to get the complete character number.
    for (LineLayoutItem layoutItem = hitLayoutItem.previousInPreOrder(queryRoot);
        layoutItem; layoutItem = layoutItem.previousInPreOrder(queryRoot)) {
        if (!layoutItem.isSVGInlineText())
            continue;
        characterNumber += LineLayoutSVGInlineText(layoutItem).resolvedTextLength();
    }
    return characterNumber;
}

static unsigned logicalOffsetInTextNode(LineLayoutSVGInlineText textLineLayout, const SVGInlineTextBox* startTextBox, unsigned fragmentOffset)
{
    Vector<SVGInlineTextBox*> textBoxes;
    collectTextBoxesInLogicalOrder(textLineLayout, textBoxes);

    ASSERT(startTextBox);
    size_t index = textBoxes.find(startTextBox);
    ASSERT(index != kNotFound);

    unsigned offset = fragmentOffset;
    while (index) {
        --index;
        offset += textBoxes[index]->len();
    }
    return offset;
}

static bool characterNumberAtPositionCallback(QueryData* queryData, const SVGTextFragment& fragment)
{
    CharacterNumberAtPositionData* data = static_cast<CharacterNumberAtPositionData*>(queryData);

    const float scalingFactor = data->textLineLayout.scalingFactor();
    ASSERT(scalingFactor);
    const float baseline = data->textLineLayout.scaledFont().getFontMetrics().floatAscent() / scalingFactor;

    // Test the query point against the bounds of the entire fragment first.
    if (!fragment.boundingBox(baseline).contains(data->position))
        return false;

    AffineTransform fragmentTransform = fragment.buildFragmentTransform(SVGTextFragment::TransformIgnoringTextLength);

    // Iterate through the glyphs in this fragment, and check if their extents
    // contain the query point.
    MetricsList::const_iterator metrics =
        data->textLineLayout.metricsList().begin() + fragment.metricsListOffset;
    unsigned fragmentOffset = 0;
    float glyphOffset = 0;
    while (fragmentOffset < fragment.length) {
        FloatPoint glyphPosition = logicalGlyphPositionToPhysical(data, fragment, glyphOffset);
        glyphPosition.move(0, -baseline);

        FloatRect extent = fragmentTransform.mapRect(physicalGlyphExtents(data, *metrics, glyphPosition));
        if (extent.contains(data->position)) {
            // Compute the character offset of the glyph within the text node.
            unsigned offsetInBox = fragment.characterOffset - queryData->textBox->start() + fragmentOffset;
            data->offsetInTextNode = logicalOffsetInTextNode(queryData->textLineLayout, queryData->textBox, offsetInBox);
            data->hitLayoutItem = LineLayoutItem(data->textLineLayout);
            return true;
        }
        fragmentOffset += metrics->length();
        glyphOffset += data->isVerticalText ? metrics->height() : metrics->width();
        ++metrics;
    }
    return false;
}

int SVGTextQuery::characterNumberAtPosition(const FloatPoint& position) const
{
    CharacterNumberAtPositionData data(position);
    spatialQuery(m_queryRootLayoutObject, &data, characterNumberAtPositionCallback);
    return data.characterNumberWithin(m_queryRootLayoutObject);
}

} // namespace blink
