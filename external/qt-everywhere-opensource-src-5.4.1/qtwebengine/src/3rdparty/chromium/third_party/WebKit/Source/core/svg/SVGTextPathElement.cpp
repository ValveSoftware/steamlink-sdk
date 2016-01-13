/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
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

#include "core/svg/SVGTextPathElement.h"

#include "core/XLinkNames.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGTextPath.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGTextPathMethodType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGTextPathMethodAlign, "align"));
        entries.append(std::make_pair(SVGTextPathMethodStretch, "stretch"));
    }
    return entries;
}

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGTextPathSpacingType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGTextPathSpacingAuto, "auto"));
        entries.append(std::make_pair(SVGTextPathSpacingExact, "exact"));
    }
    return entries;
}

inline SVGTextPathElement::SVGTextPathElement(Document& document)
    : SVGTextContentElement(SVGNames::textPathTag, document)
    , SVGURIReference(this)
    , m_startOffset(SVGAnimatedLength::create(this, SVGNames::startOffsetAttr, SVGLength::create(LengthModeOther), AllowNegativeLengths))
    , m_method(SVGAnimatedEnumeration<SVGTextPathMethodType>::create(this, SVGNames::methodAttr, SVGTextPathMethodAlign))
    , m_spacing(SVGAnimatedEnumeration<SVGTextPathSpacingType>::create(this, SVGNames::spacingAttr, SVGTextPathSpacingExact))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_startOffset);
    addToPropertyMap(m_method);
    addToPropertyMap(m_spacing);
}

DEFINE_NODE_FACTORY(SVGTextPathElement)

SVGTextPathElement::~SVGTextPathElement()
{
#if !ENABLE(OILPAN)
    clearResourceReferences();
#endif
}

void SVGTextPathElement::clearResourceReferences()
{
    document().accessSVGExtensions().removeAllTargetReferencesForElement(this);
}

bool SVGTextPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::startOffsetAttr);
        supportedAttributes.add(SVGNames::methodAttr);
        supportedAttributes.add(SVGNames::spacingAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGTextPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGTextContentElement::parseAttribute(name, value);
    else if (name == SVGNames::startOffsetAttr)
        m_startOffset->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::methodAttr)
        m_method->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::spacingAttr)
        m_spacing->setBaseValueAsString(value, parseError);
    else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGTextPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextContentElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        return;
    }

    if (attrName == SVGNames::startOffsetAttr)
        updateRelativeLengthsInformation();

    if (RenderObject* object = renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
}

RenderObject* SVGTextPathElement::createRenderer(RenderStyle*)
{
    return new RenderSVGTextPath(this);
}

bool SVGTextPathElement::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode() && (isSVGAElement(*parentNode()) || isSVGTextElement(*parentNode())))
        return Element::rendererIsNeeded(style);

    return false;
}

void SVGTextPathElement::buildPendingResource()
{
    clearResourceReferences();
    if (!inDocument())
        return;

    AtomicString id;
    Element* target = SVGURIReference::targetElementFromIRIString(hrefString(), treeScope(), &id);
    if (!target) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions().isElementPendingResource(this, id))
            return;

        if (!id.isEmpty()) {
            document().accessSVGExtensions().addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (isSVGPathElement(*target)) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions().addElementReferencingTarget(this, toSVGElement((target)));
    }
}

Node::InsertionNotificationRequest SVGTextPathElement::insertedInto(ContainerNode* rootParent)
{
    SVGTextContentElement::insertedInto(rootParent);
    buildPendingResource();
    return InsertionDone;
}

void SVGTextPathElement::removedFrom(ContainerNode* rootParent)
{
    SVGTextContentElement::removedFrom(rootParent);
    if (rootParent->inDocument())
        clearResourceReferences();
}

bool SVGTextPathElement::selfHasRelativeLengths() const
{
    return m_startOffset->currentValue()->isRelative()
        || SVGTextContentElement::selfHasRelativeLengths();
}

}
