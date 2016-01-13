/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGCursorElement.h"

#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/dom/Document.h"

namespace WebCore {

inline SVGCursorElement::SVGCursorElement(Document& document)
    : SVGElement(SVGNames::cursorTag, document)
    , SVGTests(this)
    , SVGURIReference(this)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
}

DEFINE_NODE_FACTORY(SVGCursorElement)

SVGCursorElement::~SVGCursorElement()
{
    // The below teardown is all handled by weak pointer processing in oilpan.
#if !ENABLE(OILPAN)
    HashSet<RawPtr<SVGElement> >::iterator end = m_clients.end();
    for (HashSet<RawPtr<SVGElement> >::iterator it = m_clients.begin(); it != end; ++it)
        (*it)->cursorElementRemoved();
#endif
}

bool SVGCursorElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGCursorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
    } else if (name == SVGNames::xAttr) {
        m_x->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::yAttr) {
        m_y->setBaseValueAsString(value, parseError);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else if (SVGTests::parseAttribute(name, value)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGCursorElement::addClient(SVGElement* element)
{
    m_clients.add(element);
    element->setCursorElement(this);
}

#if !ENABLE(OILPAN)
void SVGCursorElement::removeClient(SVGElement* element)
{
    HashSet<RawPtr<SVGElement> >::iterator it = m_clients.find(element);
    if (it != m_clients.end()) {
        m_clients.remove(it);
        element->cursorElementRemoved();
    }
}
#endif

void SVGCursorElement::removeReferencedElement(SVGElement* element)
{
    m_clients.remove(element);
}

void SVGCursorElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    // Any change of a cursor specific attribute triggers this recalc.
    WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = m_clients.begin();
    WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = m_clients.end();

    for (; it != end; ++it)
        (*it)->setNeedsStyleRecalc(SubtreeStyleChange);
}

void SVGCursorElement::trace(Visitor* visitor)
{
    visitor->trace(m_clients);
    SVGElement::trace(visitor);
}

}
