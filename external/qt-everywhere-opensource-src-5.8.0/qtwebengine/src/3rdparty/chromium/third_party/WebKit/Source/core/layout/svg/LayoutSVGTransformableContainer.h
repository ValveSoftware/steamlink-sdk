/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
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

#ifndef LayoutSVGTransformableContainer_h
#define LayoutSVGTransformableContainer_h

#include "core/layout/svg/LayoutSVGContainer.h"

namespace blink {

class SVGGraphicsElement;

class LayoutSVGTransformableContainer final : public LayoutSVGContainer {
public:
    explicit LayoutSVGTransformableContainer(SVGGraphicsElement*);

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGTransformableContainer || LayoutSVGContainer::isOfType(type); }
    const AffineTransform& localToSVGParentTransform() const override { return m_localTransform; }
    const FloatSize& additionalTranslation() const { return m_additionalTranslation; }

    void setNeedsTransformUpdate() override;

private:
    SVGTransformChange calculateLocalTransform() override;
    AffineTransform localSVGTransform() const override { return m_localTransform; }

    bool m_needsTransformUpdate : 1;
    AffineTransform m_localTransform;
    FloatSize m_additionalTranslation;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGTransformableContainer, isSVGTransformableContainer());

} // namespace blink

#endif // LayoutSVGTransformableContainer_h
