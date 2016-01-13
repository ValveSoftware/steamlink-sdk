/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
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

#include "core/svg/SVGImageElement.h"

#include "core/CSSPropertyNames.h"
#include "core/XLinkNames.h"
#include "core/rendering/RenderImageResource.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "core/rendering/svg/RenderSVGResource.h"

namespace WebCore {

inline SVGImageElement::SVGImageElement(Document& document)
    : SVGGraphicsElement(SVGNames::imageTag, document)
    , SVGURIReference(this)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(LengthModeWidth), ForbidNegativeLengths))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(LengthModeHeight), ForbidNegativeLengths))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
    , m_imageLoader(SVGImageLoader::create(this))
    , m_needsLoaderURIUpdate(true)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
    addToPropertyMap(m_preserveAspectRatio);
}

DEFINE_NODE_FACTORY(SVGImageElement)

void SVGImageElement::trace(Visitor* visitor)
{
    visitor->trace(m_imageLoader);
    SVGGraphicsElement::trace(visitor);
}

bool SVGImageElement::currentFrameHasSingleSecurityOrigin() const
{
    if (RenderSVGImage* renderSVGImage = toRenderSVGImage(renderer())) {
        if (renderSVGImage->imageResource()->hasImage()) {
            if (Image* image = renderSVGImage->imageResource()->cachedImage()->image())
                return image->currentFrameHasSingleSecurityOrigin();
        }
    }

    return true;
}

bool SVGImageElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

bool SVGImageElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::widthAttr || name == SVGNames::heightAttr)
        return true;
    return SVGGraphicsElement::isPresentationAttribute(name);
}

void SVGImageElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (!isSupportedAttribute(name))
        SVGGraphicsElement::collectStyleForPresentationAttribute(name, value, style);
    else if (name == SVGNames::widthAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, value);
    else if (name == SVGNames::heightAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, value);
}

void SVGImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name)) {
        SVGGraphicsElement::parseAttribute(name, value);
    } else if (name == SVGNames::xAttr) {
        m_x->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::yAttr) {
        m_y->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::widthAttr) {
        m_width->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::heightAttr) {
        m_height->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::preserveAspectRatioAttr) {
        m_preserveAspectRatio->setBaseValueAsString(value, parseError);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::xAttr
                          || attrName == SVGNames::yAttr
                          || attrName == SVGNames::widthAttr
                          || attrName == SVGNames::heightAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    if (SVGURIReference::isKnownAttribute(attrName)) {
        if (inDocument())
            imageLoader().updateFromElementIgnoringPreviousError();
        else
            m_needsLoaderURIUpdate = true;
        return;
    }

    RenderObject* renderer = this->renderer();
    if (!renderer)
        return;

    if (isLengthAttribute) {
        if (toRenderSVGImage(renderer)->updateImageViewport())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (attrName == SVGNames::preserveAspectRatioAttr) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGImageElement::selfHasRelativeLengths() const
{
    return m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative();
}

RenderObject* SVGImageElement::createRenderer(RenderStyle*)
{
    return new RenderSVGImage(this);
}

bool SVGImageElement::haveLoadedRequiredResources()
{
    return !m_needsLoaderURIUpdate && !imageLoader().hasPendingActivity();
}

void SVGImageElement::attach(const AttachContext& context)
{
    SVGGraphicsElement::attach(context);

    if (RenderSVGImage* imageObj = toRenderSVGImage(renderer())) {
        if (imageObj->imageResource()->hasImage())
            return;

        imageObj->imageResource()->setImageResource(imageLoader().image());
    }
}

Node::InsertionNotificationRequest SVGImageElement::insertedInto(ContainerNode* rootParent)
{
    SVGGraphicsElement::insertedInto(rootParent);
    if (!rootParent->inDocument())
        return InsertionDone;

    // We can only resolve base URIs properly after tree insertion - hence, URI mutations while
    // detached are deferred until this point.
    if (m_needsLoaderURIUpdate) {
        imageLoader().updateFromElementIgnoringPreviousError();
        m_needsLoaderURIUpdate = false;
    } else {
        // A previous loader update may have failed to actually fetch the image if the document
        // was inactive. In that case, force a re-update (but don't clear previous errors).
        if (!imageLoader().image())
            imageLoader().updateFromElement();
    }

    return InsertionDone;
}

const AtomicString SVGImageElement::imageSourceURL() const
{
    return AtomicString(hrefString());
}

void SVGImageElement::didMoveToNewDocument(Document& oldDocument)
{
    imageLoader().elementDidMoveToNewDocument();
    SVGGraphicsElement::didMoveToNewDocument(oldDocument);
}

}
