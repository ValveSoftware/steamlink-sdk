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

#ifndef RenderSVGTransformableContainer_h
#define RenderSVGTransformableContainer_h

#include "core/rendering/svg/RenderSVGContainer.h"

namespace WebCore {

class SVGGraphicsElement;
class RenderSVGTransformableContainer FINAL : public RenderSVGContainer {
public:
    explicit RenderSVGTransformableContainer(SVGGraphicsElement*);

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    virtual bool isSVGTransformableContainer() const OVERRIDE { return true; }
    virtual const AffineTransform& localToParentTransform() const OVERRIDE { return m_localTransform; }
    virtual void setNeedsTransformUpdate() OVERRIDE { m_needsTransformUpdate = true; }
    virtual bool didTransformToRootUpdate() OVERRIDE { return m_didTransformToRootUpdate; }

private:
    virtual bool calculateLocalTransform() OVERRIDE;
    virtual AffineTransform localTransform() const OVERRIDE { return m_localTransform; }

    bool m_needsTransformUpdate : 1;
    bool m_didTransformToRootUpdate : 1;
    AffineTransform m_localTransform;
    FloatSize m_lastTranslation;
};

}

#endif // RenderSVGTransformableContainer_h
