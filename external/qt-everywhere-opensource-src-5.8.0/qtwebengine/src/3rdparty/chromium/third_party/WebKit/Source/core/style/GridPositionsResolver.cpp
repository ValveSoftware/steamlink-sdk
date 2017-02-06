// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "GridPositionsResolver.h"

#include "core/layout/LayoutBox.h"
#include "core/style/GridArea.h"
#include <algorithm>

namespace blink {

static inline GridTrackSizingDirection directionFromSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == ColumnEndSide ? ForColumns : ForRows;
}

static inline String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return lineName + ((side == ColumnStartSide || side == RowStartSide) ? "-start" : "-end");
}

NamedLineCollection::NamedLineCollection(const ComputedStyle& gridContainerStyle, const String& namedLine, GridTrackSizingDirection direction, size_t lastLine, size_t autoRepeatTracksCount)
    : m_lastLine(lastLine)
    , m_repetitions(autoRepeatTracksCount)
{
    bool isRowAxis = direction == ForColumns;
    const NamedGridLinesMap& gridLineNames = isRowAxis ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    const NamedGridLinesMap& autoRepeatGridLineNames = isRowAxis ? gridContainerStyle.autoRepeatNamedGridColumnLines() : gridContainerStyle.autoRepeatNamedGridRowLines();

    if (!gridLineNames.isEmpty()) {
        auto it = gridLineNames.find(namedLine);
        m_namedLinesIndexes = it == gridLineNames.end() ? nullptr : &it->value;
    }

    if (!autoRepeatGridLineNames.isEmpty()) {
        auto it = autoRepeatGridLineNames.find(namedLine);
        m_autoRepeatNamedLinesIndexes = it == autoRepeatGridLineNames.end() ? nullptr : &it->value;
    }

    m_insertionPoint = isRowAxis ? gridContainerStyle.gridAutoRepeatColumnsInsertionPoint() : gridContainerStyle.gridAutoRepeatRowsInsertionPoint();
}

bool NamedLineCollection::isValidNamedLineOrArea(const String& namedLine, const ComputedStyle& gridContainerStyle, GridPositionSide side)
{
    bool isRowAxis = directionFromSide(side) == ForColumns;
    const NamedGridLinesMap& gridLineNames = isRowAxis ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    const NamedGridLinesMap& autoRepeatGridLineNames = isRowAxis ? gridContainerStyle.autoRepeatNamedGridColumnLines() : gridContainerStyle.autoRepeatNamedGridRowLines();

    if (gridLineNames.contains(namedLine) || autoRepeatGridLineNames.contains(namedLine))
        return true;

    String implicitName = implicitNamedGridLineForSide(namedLine, side);
    return gridLineNames.contains(implicitName) || autoRepeatGridLineNames.contains(implicitName);
}

bool NamedLineCollection::hasNamedLines()
{
    return m_namedLinesIndexes || m_autoRepeatNamedLinesIndexes;
}

size_t NamedLineCollection::find(size_t line)
{
    if (line > m_lastLine)
        return kNotFound;

    if (!m_autoRepeatNamedLinesIndexes || line < m_insertionPoint)
        return m_namedLinesIndexes ? m_namedLinesIndexes->find(line) : kNotFound;

    if (line <= (m_insertionPoint + m_repetitions)) {
        size_t localIndex = line - m_insertionPoint;

        // The line names defined in the last line are also present in the first line of the next
        // repetition (if any). Same for the line names defined in the first line.  Note that there
        // is only one auto-repeated track allowed by the syntax, that's why it's enough to store
        // indexes 0 and 1 (before and after the track size).
        if (localIndex == m_repetitions)
            return m_autoRepeatNamedLinesIndexes->find(static_cast<size_t>(1));
        size_t position = m_autoRepeatNamedLinesIndexes->find(static_cast<size_t>(0));
        if (position != kNotFound)
            return position;
        return localIndex == 0 ? kNotFound : m_autoRepeatNamedLinesIndexes->find(static_cast<size_t>(1));
    }

    return m_namedLinesIndexes ? m_namedLinesIndexes->find(line - (m_repetitions - 1)) : kNotFound;
}

bool NamedLineCollection::contains(size_t line)
{
    CHECK(hasNamedLines());
    return find(line) != kNotFound;
}

size_t NamedLineCollection::firstPosition()
{
    CHECK(hasNamedLines());

    size_t firstLine = 0;

    if (!m_autoRepeatNamedLinesIndexes) {
        if (m_insertionPoint == 0 || m_insertionPoint < m_namedLinesIndexes->at(firstLine))
            return m_namedLinesIndexes->at(firstLine) + (m_repetitions ? m_repetitions - 1 : 0);
        return m_namedLinesIndexes->at(firstLine);
    }

    if (!m_namedLinesIndexes)
        return m_autoRepeatNamedLinesIndexes->at(firstLine) + m_insertionPoint;

    if (m_insertionPoint == 0)
        return m_autoRepeatNamedLinesIndexes->at(firstLine);

    return std::min(m_namedLinesIndexes->at(firstLine), m_autoRepeatNamedLinesIndexes->at(firstLine) + m_insertionPoint);
}

GridPositionSide GridPositionsResolver::initialPositionSide(GridTrackSizingDirection direction)
{
    return (direction == ForColumns) ? ColumnStartSide : RowStartSide;
}

GridPositionSide GridPositionsResolver::finalPositionSide(GridTrackSizingDirection direction)
{
    return (direction == ForColumns) ? ColumnEndSide : RowEndSide;
}

static void initialAndFinalPositionsFromStyle(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction, GridPosition& initialPosition, GridPosition& finalPosition)
{
    initialPosition = (direction == ForColumns) ? gridItem.style()->gridColumnStart() : gridItem.style()->gridRowStart();
    finalPosition = (direction == ForColumns) ? gridItem.style()->gridColumnEnd() : gridItem.style()->gridRowEnd();

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    if (gridItem.isOutOfFlowPositioned()) {
        // Early detect the case of non existing named grid lines for positioned items.
        if (initialPosition.isNamedGridArea() && !NamedLineCollection::isValidNamedLineOrArea(initialPosition.namedGridLine(), gridContainerStyle, GridPositionsResolver::initialPositionSide(direction)))
            initialPosition.setAutoPosition();

        if (finalPosition.isNamedGridArea() && !NamedLineCollection::isValidNamedLineOrArea(finalPosition.namedGridLine(), gridContainerStyle, GridPositionsResolver::finalPositionSide(direction)))
            finalPosition.setAutoPosition();
    }

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition.setSpanPosition(1, String());
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition.setSpanPosition(1, String());
}

static size_t lookAheadForNamedGridLine(int start, size_t numberOfLines, size_t gridLastLine, NamedLineCollection& linesCollection)
{
    ASSERT(numberOfLines);

    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from first line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    size_t end = std::max(start, 0);

    if (!linesCollection.hasNamedLines()) {
        end = std::max(end, gridLastLine + 1);
        return end + numberOfLines - 1;
    }

    for (; numberOfLines; ++end) {
        if (end > gridLastLine || linesCollection.contains(end))
            numberOfLines--;
    }

    ASSERT(end);
    return end - 1;
}

static int lookBackForNamedGridLine(int end, size_t numberOfLines, int gridLastLine, NamedLineCollection& linesCollection)
{
    ASSERT(numberOfLines);

    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from last line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    int start = std::min(end, gridLastLine);

    if (!linesCollection.hasNamedLines()) {
        start = std::min(start, -1);
        return start - numberOfLines + 1;
    }

    for (; numberOfLines; --start) {
        if (start < 0 || linesCollection.contains(start))
            numberOfLines--;
    }

    return start + 1;
}

static GridSpan definiteGridSpanWithNamedSpanAgainstOpposite(int oppositeLine, const GridPosition& position, GridPositionSide side, int lastLine, NamedLineCollection& linesCollection)
{
    int start, end;
    if (side == RowStartSide || side == ColumnStartSide) {
        start = lookBackForNamedGridLine(oppositeLine - 1, position.spanPosition(), lastLine, linesCollection);
        end = oppositeLine;
    } else {
        start = oppositeLine;
        end = lookAheadForNamedGridLine(oppositeLine + 1, position.spanPosition(), lastLine, linesCollection);
    }

    return GridSpan::untranslatedDefiniteGridSpan(start, end);
}

size_t GridPositionsResolver::explicitGridColumnCount(const ComputedStyle& gridContainerStyle, size_t autoRepeatTracksCount)
{
    return std::min<size_t>(std::max(gridContainerStyle.gridTemplateColumns().size() + autoRepeatTracksCount, gridContainerStyle.namedGridAreaColumnCount()), kGridMaxTracks);
}

size_t GridPositionsResolver::explicitGridRowCount(const ComputedStyle& gridContainerStyle, size_t autoRepeatTracksCount)
{
    return std::min<size_t>(std::max(gridContainerStyle.gridTemplateRows().size() + autoRepeatTracksCount, gridContainerStyle.namedGridAreaRowCount()), kGridMaxTracks);
}

static size_t explicitGridSizeForSide(const ComputedStyle& gridContainerStyle, GridPositionSide side, size_t autoRepeatTracksCount)
{
    return (side == ColumnStartSide || side == ColumnEndSide) ? GridPositionsResolver::explicitGridColumnCount(gridContainerStyle, autoRepeatTracksCount) : GridPositionsResolver::explicitGridRowCount(gridContainerStyle, autoRepeatTracksCount);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const ComputedStyle& gridContainerStyle, int oppositeLine, const GridPosition& position, size_t autoRepeatTracksCount, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side, autoRepeatTracksCount);
    NamedLineCollection linesCollection(gridContainerStyle, position.namedGridLine(), directionFromSide(side), lastLine, autoRepeatTracksCount);
    return definiteGridSpanWithNamedSpanAgainstOpposite(oppositeLine, position, side, lastLine, linesCollection);
}

static GridSpan definiteGridSpanWithSpanAgainstOpposite(int oppositeLine, const GridPosition& position, GridPositionSide side)
{
    size_t positionOffset = position.spanPosition();
    if (side == ColumnStartSide || side == RowStartSide)
        return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - positionOffset, oppositeLine);

    return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + positionOffset);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const ComputedStyle& gridContainerStyle, int oppositeLine, const GridPosition& position, GridPositionSide side, size_t autoRepeatTracksCount)
{
    if (position.isAuto()) {
        if (side == ColumnStartSide || side == RowStartSide)
            return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - 1, oppositeLine);
        return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + 1);
    }

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainerStyle, oppositeLine, position, autoRepeatTracksCount, side);
    }

    return definiteGridSpanWithSpanAgainstOpposite(oppositeLine, position, side);
}

size_t GridPositionsResolver::spanSizeForAutoPlacedItem(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    initialAndFinalPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    if (initialPosition.isAuto() && finalPosition.isAuto())
        return 1;

    GridPosition position = initialPosition.isSpan() ? initialPosition : finalPosition;
    ASSERT(position.isSpan());
    ASSERT(position.spanPosition());
    return position.spanPosition();
}

static int resolveNamedGridLinePositionFromStyle(const ComputedStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side, size_t autoRepeatTracksCount)
{
    ASSERT(!position.namedGridLine().isNull());

    size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side, autoRepeatTracksCount);
    NamedLineCollection linesCollection(gridContainerStyle, position.namedGridLine(), directionFromSide(side), lastLine, autoRepeatTracksCount);

    if (position.isPositive())
        return lookAheadForNamedGridLine(0, abs(position.integerPosition()), lastLine, linesCollection);

    return lookBackForNamedGridLine(lastLine, abs(position.integerPosition()), lastLine, linesCollection);
}

static int resolveGridPositionFromStyle(const ComputedStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side, size_t autoRepeatTracksCount)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side, autoRepeatTracksCount);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return position.integerPosition() - 1;

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        size_t endOfTrack = explicitGridSizeForSide(gridContainerStyle, side, autoRepeatTracksCount);

        return endOfTrack - resolvedPosition;
    }
    case NamedGridAreaPosition:
    {
        // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
        // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
        // line to the grid item's placement.
        String namedGridLine = position.namedGridLine();
        ASSERT(!position.namedGridLine().isNull());

        size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side, autoRepeatTracksCount);
        NamedLineCollection implicitLines(gridContainerStyle, implicitNamedGridLineForSide(namedGridLine, side), directionFromSide(side), lastLine, autoRepeatTracksCount);
        if (implicitLines.hasNamedLines())
            return implicitLines.firstPosition();

        // Otherwise, if there is a named line with the specified name, contributes the first such line to the grid
        // item's placement.
        NamedLineCollection explicitLines(gridContainerStyle, namedGridLine, directionFromSide(side), lastLine, autoRepeatTracksCount);
        if (explicitLines.hasNamedLines())
            return explicitLines.firstPosition();

        ASSERT(!NamedLineCollection::isValidNamedLineOrArea(namedGridLine, gridContainerStyle, side));
        // If none of the above works specs mandate to assume that all the lines in the implicit grid have this name.
        return lastLine + 1;
    }
    case AutoPosition:
    case SpanPosition:
        // 'auto' and span depend on the opposite position for resolution (e.g. grid-row: auto / 1 or grid-column: span 3 / "myHeader").
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

GridSpan GridPositionsResolver::resolveGridPositionsFromStyle(const ComputedStyle& gridContainerStyle, const LayoutBox& gridItem, GridTrackSizingDirection direction, size_t autoRepeatTracksCount)
{
    GridPosition initialPosition, finalPosition;
    initialAndFinalPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    GridPositionSide initialSide = initialPositionSide(direction);
    GridPositionSide finalSide = finalPositionSide(direction);

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // We can't get our grid positions without running the auto placement algorithm.
        return GridSpan::indefiniteGridSpan();
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        int endLine = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide, autoRepeatTracksCount);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, endLine, initialPosition, initialSide, autoRepeatTracksCount);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        int startLine = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide, autoRepeatTracksCount);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, startLine, finalPosition, finalSide, autoRepeatTracksCount);
    }

    int startLine = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide, autoRepeatTracksCount);
    int endLine = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide, autoRepeatTracksCount);

    if (endLine < startLine)
        std::swap(endLine, startLine);
    else if (endLine == startLine)
        endLine = startLine + 1;

    return GridSpan::untranslatedDefiniteGridSpan(startLine, endLine);
}

} // namespace blink
