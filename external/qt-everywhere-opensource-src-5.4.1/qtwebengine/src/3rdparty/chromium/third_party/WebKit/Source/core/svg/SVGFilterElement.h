/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedInteger.h"
#include "core/svg/SVGAnimatedIntegerOptionalInteger.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"

namespace WebCore {

class SVGFilterElement FINAL : public SVGElement,
                               public SVGURIReference {
public:
    DECLARE_NODE_FACTORY(SVGFilterElement);
    virtual void trace(Visitor*) OVERRIDE;

    void setFilterRes(unsigned x, unsigned y);
    void addClient(Node*);
    void removeClient(Node*);

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* filterUnits() { return m_filterUnits.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* primitiveUnits() { return m_primitiveUnits.get(); }
    SVGAnimatedInteger* filterResX() { return m_filterRes->firstInteger(); }
    SVGAnimatedInteger* filterResY() { return m_filterRes->secondInteger(); }

private:
    explicit SVGFilterElement(Document&);

    virtual bool needsPendingResourceHandling() const OVERRIDE { return false; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    RefPtr<SVGAnimatedLength> m_x;
    RefPtr<SVGAnimatedLength> m_y;
    RefPtr<SVGAnimatedLength> m_width;
    RefPtr<SVGAnimatedLength> m_height;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_filterUnits;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_primitiveUnits;
    RefPtr<SVGAnimatedIntegerOptionalInteger> m_filterRes;

    WillBeHeapHashSet<RefPtrWillBeMember<Node> > m_clientsToAdd;
};

}

#endif
