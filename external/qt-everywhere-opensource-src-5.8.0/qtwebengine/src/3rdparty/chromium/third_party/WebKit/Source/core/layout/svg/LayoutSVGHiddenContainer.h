/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef LayoutSVGHiddenContainer_h
#define LayoutSVGHiddenContainer_h

#include "core/layout/svg/LayoutSVGContainer.h"

namespace blink {

class SVGElement;

// This class is for containers which are never drawn, but do need to support style
// <defs>, <linearGradient>, <radialGradient> are all good examples
class LayoutSVGHiddenContainer : public LayoutSVGContainer {
public:
    explicit LayoutSVGHiddenContainer(SVGElement*);

    const char* name() const override { return "LayoutSVGHiddenContainer"; }

protected:
    void layout() override;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGHiddenContainer || LayoutSVGContainer::isOfType(type); }

private:
    void paint(const PaintInfo&, const LayoutPoint&) const final;
    LayoutRect absoluteClippedOverflowRect() const final { return LayoutRect(); }
    FloatRect paintInvalidationRectInLocalSVGCoordinates() const final { return FloatRect(); }
    void absoluteQuads(Vector<FloatQuad>&) const final;

    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) final;
};
} // namespace blink

#endif // LayoutSVGHiddenContainer_h
