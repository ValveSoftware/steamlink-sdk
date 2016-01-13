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

#include "core/svg/SVGScriptElement.h"

#include "bindings/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/XLinkNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"

namespace WebCore {

inline SVGScriptElement::SVGScriptElement(Document& document, bool wasInsertedByParser, bool alreadyStarted)
    : SVGElement(SVGNames::scriptTag, document)
    , SVGURIReference(this)
    , m_svgLoadEventTimer(this, &SVGElement::svgLoadEventTimerFired)
    , m_loader(ScriptLoader::create(this, wasInsertedByParser, alreadyStarted))
{
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<SVGScriptElement> SVGScriptElement::create(Document& document, bool insertedByParser)
{
    return adoptRefWillBeNoop(new SVGScriptElement(document, insertedByParser, false));
}

bool SVGScriptElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::typeAttr);
        supportedAttributes.add(HTMLNames::onerrorAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGScriptElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;
    if (name == SVGNames::typeAttr)
        return;

    if (name == HTMLNames::onerrorAttr) {
        setAttributeEventListener(EventTypeNames::error, createAttributeEventListener(this, name, value, eventParameterName()));
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGScriptElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::typeAttr || attrName == HTMLNames::onerrorAttr)
        return;

    if (SVGURIReference::isKnownAttribute(attrName)) {
        m_loader->handleSourceAttribute(hrefString());
        return;
    }

    ASSERT_NOT_REACHED();
}

Node::InsertionNotificationRequest SVGScriptElement::insertedInto(ContainerNode* rootParent)
{
    SVGElement::insertedInto(rootParent);
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGScriptElement::didNotifySubtreeInsertionsToDocument()
{
    m_loader->didNotifySubtreeInsertionsToDocument();

    if (!m_loader->isParserInserted()) {
        m_loader->setHaveFiredLoadEvent(true);
        sendSVGLoadEventIfPossibleAsynchronously();
    }
}

void SVGScriptElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    m_loader->childrenChanged();
}

bool SVGScriptElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == AtomicString(sourceAttributeValue());
}

void SVGScriptElement::finishParsingChildren()
{
    SVGElement::finishParsingChildren();
    m_loader->setHaveFiredLoadEvent(true);
}

bool SVGScriptElement::haveLoadedRequiredResources()
{
    return m_loader->haveFiredLoadEvent();
}

String SVGScriptElement::sourceAttributeValue() const
{
    return hrefString();
}

String SVGScriptElement::charsetAttributeValue() const
{
    return String();
}

String SVGScriptElement::typeAttributeValue() const
{
    return getAttribute(SVGNames::typeAttr).string();
}

String SVGScriptElement::languageAttributeValue() const
{
    return String();
}

String SVGScriptElement::forAttributeValue() const
{
    return String();
}

String SVGScriptElement::eventAttributeValue() const
{
    return String();
}

bool SVGScriptElement::asyncAttributeValue() const
{
    return false;
}

bool SVGScriptElement::deferAttributeValue() const
{
    return false;
}

bool SVGScriptElement::hasSourceAttribute() const
{
    return href()->isSpecified();
}

PassRefPtrWillBeRawPtr<Element> SVGScriptElement::cloneElementWithoutAttributesAndChildren()
{
    return adoptRefWillBeNoop(new SVGScriptElement(document(), false, m_loader->alreadyStarted()));
}

void SVGScriptElement::dispatchLoadEvent()
{
    dispatchEvent(Event::create(EventTypeNames::load));
}

#ifndef NDEBUG
bool SVGScriptElement::isAnimatableAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::typeAttr)
        return false;

    return SVGElement::isAnimatableAttribute(name);
}
#endif

}
