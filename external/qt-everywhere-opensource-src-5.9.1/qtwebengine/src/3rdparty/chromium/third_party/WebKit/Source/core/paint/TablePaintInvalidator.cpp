// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TablePaintInvalidator.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason TablePaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_table, m_context).invalidatePaintIfNeeded();

  // Table cells paint background from the containing column group, column,
  // section and row.  If background of any of them changed, we need to
  // invalidate all affected cells.  Here use shouldDoFullPaintInvalidation() as
  // a broader condition of background change.

  // If any col changed background, we'll check all cells for background
  // changes.
  bool hasColChangedBackground = false;
  for (LayoutTableCol* col = m_table.firstColumn(); col;
       col = col->nextColumn()) {
    // This ensures that the backgroundChangedSinceLastPaintInvalidation flag
    // is up-to-date.
    col->ensureIsReadyForPaintInvalidation();
    if (col->backgroundChangedSinceLastPaintInvalidation()) {
      hasColChangedBackground = true;
      break;
    }
  }
  for (LayoutObject* child = m_table.firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isTableSection())
      continue;
    LayoutTableSection* section = toLayoutTableSection(child);
    section->ensureIsReadyForPaintInvalidation();
    ObjectPaintInvalidator sectionInvalidator(*section);
    if (!hasColChangedBackground &&
        !section
             ->shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState())
      continue;
    for (LayoutTableRow* row = section->firstRow(); row; row = row->nextRow()) {
      row->ensureIsReadyForPaintInvalidation();
      if (!hasColChangedBackground &&
          !section->backgroundChangedSinceLastPaintInvalidation() &&
          !row->backgroundChangedSinceLastPaintInvalidation())
        continue;
      for (LayoutTableCell* cell = row->firstCell(); cell;
           cell = cell->nextCell()) {
        cell->ensureIsReadyForPaintInvalidation();
        bool invalidated = false;
        // Table cells paint container's background on the container's backing
        // instead of its own (if any), so we must invalidate it by the
        // containers.
        if (section->backgroundChangedSinceLastPaintInvalidation()) {
          sectionInvalidator
              .slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
                  cell->backgroundDisplayItemClient(),
                  PaintInvalidationStyleChange);
          invalidated = true;
        } else if (hasColChangedBackground) {
          LayoutTable::ColAndColGroup colAndColGroup =
              m_table.colElementAtAbsoluteColumn(cell->absoluteColumnIndex());
          LayoutTableCol* column = colAndColGroup.col;
          LayoutTableCol* columnGroup = colAndColGroup.colgroup;
          if ((columnGroup &&
               columnGroup->backgroundChangedSinceLastPaintInvalidation()) ||
              (column &&
               column->backgroundChangedSinceLastPaintInvalidation())) {
            sectionInvalidator
                .slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
                    cell->backgroundDisplayItemClient(),
                    PaintInvalidationStyleChange);
            invalidated = true;
          }
        }
        if ((!invalidated || row->hasSelfPaintingLayer()) &&
            row->backgroundChangedSinceLastPaintInvalidation()) {
          ObjectPaintInvalidator(*row)
              .slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
                  cell->backgroundDisplayItemClient(),
                  PaintInvalidationStyleChange);
        }
      }
    }
  }

  return reason;
}

}  // namespace blink
