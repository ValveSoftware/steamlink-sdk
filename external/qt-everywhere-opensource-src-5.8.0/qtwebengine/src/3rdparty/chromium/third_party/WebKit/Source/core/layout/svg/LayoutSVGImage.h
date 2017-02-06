/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef LayoutSVGImage_h
#define LayoutSVGImage_h

#include "core/layout/svg/LayoutSVGModelObject.h"

namespace blink {

class LayoutImageResource;
class SVGImageElement;

class LayoutSVGImage final : public LayoutSVGModelObject {
public:
    explicit LayoutSVGImage(SVGImageElement*);
    ~LayoutSVGImage() override;

    void setNeedsBoundariesUpdate() override { m_needsBoundariesUpdate = true; }
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    LayoutImageResource* imageResource() { return m_imageResource.get(); }
    const LayoutImageResource* imageResource() const { return m_imageResource.get(); }

    const AffineTransform& localToSVGParentTransform() const override { return m_localTransform; }

    FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGImage || LayoutSVGModelObject::isOfType(type); }

    const char* name() const override { return "LayoutSVGImage"; }

protected:
    void willBeDestroyed() override;

private:
    FloatRect strokeBoundingBox() const override { return m_objectBoundingBox; }

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const override;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    void layout() override;
    void paint(const PaintInfo&, const LayoutPoint&) const override;

    void updateBoundingBox();

    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    AffineTransform localSVGTransform() const override { return m_localTransform; }

    bool m_needsBoundariesUpdate : 1;
    bool m_needsTransformUpdate : 1;
    AffineTransform m_localTransform;
    FloatRect m_objectBoundingBox;
    Persistent<LayoutImageResource> m_imageResource;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGImage, isSVGImage());

} // namespace blink

#endif // LayoutSVGImage_h
