/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "config.h"

#include "core/svg/SVGViewElement.h"

namespace WebCore {

inline SVGViewElement::SVGViewElement(Document& document)
    : SVGElement(SVGNames::viewTag, document)
    , SVGFitToViewBox(this)
    , m_viewTarget(SVGStaticStringList::create(this, SVGNames::viewTargetAttr))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_viewTarget);
}

DEFINE_NODE_FACTORY(SVGViewElement)

bool SVGViewElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        SVGZoomAndPan::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::viewTargetAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGViewElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (SVGFitToViewBox::parseAttribute(name, value, document(), parseError)) {
    } else if (SVGZoomAndPan::parseAttribute(name, value)) {
    } else if (name == SVGNames::viewTargetAttr) {
        m_viewTarget->setBaseValueAsString(value, parseError);
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

}
