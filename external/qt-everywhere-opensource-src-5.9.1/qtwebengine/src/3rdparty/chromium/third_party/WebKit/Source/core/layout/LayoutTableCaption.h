/*
 * Copyright (C) 2011 Robert Hogan <robert@roberthogan.net>
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

#ifndef LayoutTableCaption_h
#define LayoutTableCaption_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

class LayoutTable;

// LayoutTableCaption is used to represent a table caption
// (display: table-caption).
//
// Table captions are full block-flows, which is why they inherit from
// LayoutBlockFlow. This is the only internal table part that allows margins.
// All other elements simply ignore it to satisfy the table model.
//
// This class is very simple as the logic for handling the caption is done in
// LayoutTable. In particular, the placement and sizing is done in
// LayoutTable::layoutCaption. The function is called at different timing
// depending on the 'caption-side' property: "top" is laid out before table row
// groups when "bottom" ones are laid out after. This ensures that "top"
// captions are visually before the row groups and "bottom" ones are after.
//
// See http://www.w3.org/TR/CSS21/tables.html#caption-position for the
// positioning.
class LayoutTableCaption final : public LayoutBlockFlow {
 public:
  explicit LayoutTableCaption(Element*);
  ~LayoutTableCaption() override;
  LayoutUnit containingBlockLogicalWidthForContent() const override;

 private:
  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectTableCaption || LayoutBlockFlow::isOfType(type);
  }

  void insertedIntoTree() override;
  void willBeRemovedFromTree() override;

  LayoutTable* table() const;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableCaption, isTableCaption());

}  // namespace blink

#endif  // LayoutTableCaption_h
