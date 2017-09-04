// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridPositionsResolver_h
#define GridPositionsResolver_h

#include "core/style/GridPosition.h"
#include "wtf/Allocator.h"

namespace blink {

struct GridSpan;
class LayoutBox;
class ComputedStyle;

enum GridPositionSide {
  ColumnStartSide,
  ColumnEndSide,
  RowStartSide,
  RowEndSide
};

enum GridTrackSizingDirection { ForColumns, ForRows };

class NamedLineCollection {
  WTF_MAKE_NONCOPYABLE(NamedLineCollection);

 public:
  NamedLineCollection(const ComputedStyle&,
                      const String& namedLine,
                      GridTrackSizingDirection,
                      size_t lastLine,
                      size_t autoRepeatTracksCount);

  static bool isValidNamedLineOrArea(const String& namedLine,
                                     const ComputedStyle&,
                                     GridPositionSide);

  bool hasNamedLines();
  size_t firstPosition();

  bool contains(size_t line);

 private:
  size_t find(size_t line);
  const Vector<size_t>* m_namedLinesIndexes = nullptr;
  const Vector<size_t>* m_autoRepeatNamedLinesIndexes = nullptr;

  size_t m_insertionPoint;
  size_t m_lastLine;
  size_t m_autoRepeatTotalTracks;
  size_t m_autoRepeatTrackListLength;
};

// This is a utility class with all the code related to grid items positions
// resolution.
class GridPositionsResolver {
  DISALLOW_NEW();

 public:
  static size_t explicitGridColumnCount(const ComputedStyle&,
                                        size_t autoRepeatColumnsCount);
  static size_t explicitGridRowCount(const ComputedStyle&,
                                     size_t autoRepeatRowsCount);

  static GridPositionSide initialPositionSide(GridTrackSizingDirection);
  static GridPositionSide finalPositionSide(GridTrackSizingDirection);

  static size_t spanSizeForAutoPlacedItem(const ComputedStyle&,
                                          const LayoutBox&,
                                          GridTrackSizingDirection);
  static GridSpan resolveGridPositionsFromStyle(const ComputedStyle&,
                                                const LayoutBox&,
                                                GridTrackSizingDirection,
                                                size_t autoRepeatTracksCount);
};

}  // namespace blink

#endif  // GridPositionsResolver_h
