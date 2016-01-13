/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGFEImageElement.h"

#include "core/XLinkNames.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "platform/graphics/Image.h"

namespace WebCore {

inline SVGFEImageElement::SVGFEImageElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feImageTag, document)
    , SVGURIReference(this)
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
{
    ScriptWrappable::init(this);
    addToPropertyMap(m_preserveAspectRatio);
}

DEFINE_NODE_FACTORY(SVGFEImageElement)

SVGFEImageElement::~SVGFEImageElement()
{
#if ENABLE(OILPAN)
    if (m_cachedImage) {
        m_cachedImage->removeClient(this);
        m_cachedImage = 0;
    }
#else
    clearResourceReferences();
#endif
}

bool SVGFEImageElement::currentFrameHasSingleSecurityOrigin() const
{
    if (m_cachedImage && m_cachedImage->image())
        return m_cachedImage->image()->currentFrameHasSingleSecurityOrigin();

    return true;
}

void SVGFEImageElement::clearResourceReferences()
{
    if (m_cachedImage) {
        m_cachedImage->removeClient(this);
        m_cachedImage = 0;
    }

    document().accessSVGExtensions().removeAllTargetReferencesForElement(this);
}

void SVGFEImageElement::fetchImageResource()
{
    FetchRequest request(ResourceRequest(ownerDocument()->completeURL(hrefString())), localName());
    m_cachedImage = document().fetcher()->fetchImage(request);

    if (m_cachedImage)
        m_cachedImage->addClient(this);
}

void SVGFEImageElement::buildPendingResource()
{
    clearResourceReferences();
    if (!inDocument())
        return;

    AtomicString id;
    Element* target = SVGURIReference::targetElementFromIRIString(hrefString(), treeScope(), &id);
    if (!target) {
        if (id.isEmpty())
            fetchImageResource();
        else {
            document().accessSVGExtensions().addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (target->isSVGElement()) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions().addElementReferencingTarget(this, toSVGElement(target));
    }

    invalidate();
}

bool SVGFEImageElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::preserveAspectRatioAttr) {
        m_preserveAspectRatio->setBaseValueAsString(value, parseError);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGFEImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::preserveAspectRatioAttr) {
        invalidate();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        return;
    }

    ASSERT_NOT_REACHED();
}

Node::InsertionNotificationRequest SVGFEImageElement::insertedInto(ContainerNode* rootParent)
{
    SVGFilterPrimitiveStandardAttributes::insertedInto(rootParent);
    buildPendingResource();
    return InsertionDone;
}

void SVGFEImageElement::removedFrom(ContainerNode* rootParent)
{
    SVGFilterPrimitiveStandardAttributes::removedFrom(rootParent);
    if (rootParent->inDocument())
        clearResourceReferences();
}

void SVGFEImageElement::notifyFinished(Resource*)
{
    if (!inDocument())
        return;

    Element* parent = parentElement();
    ASSERT(parent);

    if (!isSVGFilterElement(*parent) || !parent->renderer())
        return;

    if (RenderObject* renderer = this->renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
}

PassRefPtr<FilterEffect> SVGFEImageElement::build(SVGFilterBuilder*, Filter* filter)
{
    if (m_cachedImage)
        return FEImage::createWithImage(filter, m_cachedImage->imageForRenderer(renderer()), m_preserveAspectRatio->currentValue());
    return FEImage::createWithIRIReference(filter, treeScope(), hrefString(), m_preserveAspectRatio->currentValue());
}

}
