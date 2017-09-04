// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableSectionPainter_h
#define TableSectionPainter_h

#include "core/paint/PaintPhase.h"
#include "core/style/ShadowData.h"
#include "wtf/Allocator.h"

namespace blink {

class CellSpan;
class CollapsedBorderValue;
class LayoutPoint;
class LayoutTableCell;
class LayoutTableSection;
struct PaintInfo;

class TableSectionPainter {
  STACK_ALLOCATED();

 public:
  TableSectionPainter(const LayoutTableSection& layoutTableSection)
      : m_layoutTableSection(layoutTableSection) {}

  void paint(const PaintInfo&, const LayoutPoint&);
  void paintCollapsedBorders(const PaintInfo&,
                             const LayoutPoint&,
                             const CollapsedBorderValue&);

 private:
  void paintObject(const PaintInfo&, const LayoutPoint&);

  void paintBackgroundsBehindCell(const LayoutTableCell&,
                                  const PaintInfo&,
                                  const LayoutPoint&);
  void paintCell(const LayoutTableCell&, const PaintInfo&, const LayoutPoint&);
  void paintBoxShadow(const PaintInfo&, const LayoutPoint&, ShadowStyle);

  // Returns the primary cell that should be painted for the grid item at (row,
  // column) intersecting dirtiedRows and dirtiedColumns. Returns nullptr if we
  // have painted the grid item when painting the grid item left to or above
  // (row, column) when painting cells intersecting dirtiedRows and
  // dirtiedColumns.
  const LayoutTableCell* primaryCellToPaint(
      unsigned row,
      unsigned column,
      const CellSpan& dirtiedRows,
      const CellSpan& dirtiedColumns) const;

  enum ItemToPaint { PaintCollapsedBorders, PaintSection };
  void paintRepeatingHeaderGroup(const PaintInfo&,
                                 const LayoutPoint& paintOffset,
                                 const CollapsedBorderValue& currentBorderValue,
                                 ItemToPaint);
  void paintSection(const PaintInfo&, const LayoutPoint&);
  void paintCollapsedSectionBorders(const PaintInfo&,
                                    const LayoutPoint&,
                                    const CollapsedBorderValue&);

  const LayoutTableSection& m_layoutTableSection;
};

}  // namespace blink

#endif  // TableSectionPainter_h
