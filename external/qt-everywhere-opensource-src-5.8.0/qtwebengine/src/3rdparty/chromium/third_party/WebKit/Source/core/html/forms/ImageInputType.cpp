/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 *
 */

#include "core/html/forms/ImageInputType.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/MouseEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageFallbackHelper.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutImage.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

inline ImageInputType::ImageInputType(HTMLInputElement& element)
    : BaseButtonInputType(element)
    , m_useFallbackContent(false)
{
}

InputType* ImageInputType::create(HTMLInputElement& element)
{
    return new ImageInputType(element);
}

const AtomicString& ImageInputType::formControlType() const
{
    return InputTypeNames::image;
}

bool ImageInputType::isFormDataAppendable() const
{
    return true;
}

void ImageInputType::appendToFormData(FormData& formData) const
{
    if (!element().isActivatedSubmit())
        return;
    const AtomicString& name = element().name();
    if (name.isEmpty()) {
        formData.append("x", m_clickLocation.x());
        formData.append("y", m_clickLocation.y());
        return;
    }

    DEFINE_STATIC_LOCAL(String, dotXString, (".x"));
    DEFINE_STATIC_LOCAL(String, dotYString, (".y"));
    formData.append(name + dotXString, m_clickLocation.x());
    formData.append(name + dotYString, m_clickLocation.y());

    if (!element().value().isEmpty())
        formData.append(name, element().value());
}

String ImageInputType::resultForDialogSubmit() const
{
    StringBuilder result;
    result.appendNumber(m_clickLocation.x());
    result.append(',');
    result.appendNumber(m_clickLocation.y());
    return result.toString();
}

bool ImageInputType::supportsValidation() const
{
    return false;
}

static IntPoint extractClickLocation(Event* event)
{
    if (!event->underlyingEvent() || !event->underlyingEvent()->isMouseEvent())
        return IntPoint();
    MouseEvent* mouseEvent = toMouseEvent(event->underlyingEvent());
    if (!mouseEvent->hasPosition())
        return IntPoint();
    return IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY());
}

void ImageInputType::handleDOMActivateEvent(Event* event)
{
    if (element().isDisabledFormControl() || !element().form())
        return;
    element().setActivatedSubmit(true);
    m_clickLocation = extractClickLocation(event);
    element().form()->prepareForSubmission(event); // Event handlers can run.
    element().setActivatedSubmit(false);
    event->setDefaultHandled();
}

LayoutObject* ImageInputType::createLayoutObject(const ComputedStyle& style) const
{
    if (m_useFallbackContent)
        return new LayoutBlockFlow(&element());
    LayoutImage* image = new LayoutImage(&element());
    image->setImageResource(LayoutImageResource::create());
    return image;
}

void ImageInputType::altAttributeChanged()
{
    if (element().userAgentShadowRoot()) {
        Element* text = element().userAgentShadowRoot()->getElementById("alttext");
        String value = element().altText();
        if (text && text->textContent() != value)
            text->setTextContent(element().altText());
    }
}

void ImageInputType::srcAttributeChanged()
{
    if (!element().layoutObject())
        return;
    element().ensureImageLoader().updateFromElement(ImageLoader::UpdateIgnorePreviousError);
}

void ImageInputType::valueAttributeChanged()
{
    if (m_useFallbackContent)
        return;
    BaseButtonInputType::valueAttributeChanged();
}

void ImageInputType::startResourceLoading()
{
    BaseButtonInputType::startResourceLoading();

    HTMLImageLoader& imageLoader = element().ensureImageLoader();
    imageLoader.updateFromElement();

    LayoutObject* layoutObject = element().layoutObject();
    if (!layoutObject || !layoutObject->isLayoutImage())
        return;

    LayoutImageResource* imageResource = toLayoutImage(layoutObject)->imageResource();
    imageResource->setImageResource(imageLoader.image());
}

bool ImageInputType::shouldRespectAlignAttribute()
{
    return true;
}

bool ImageInputType::canBeSuccessfulSubmitButton()
{
    return true;
}

bool ImageInputType::isEnumeratable()
{
    return false;
}

bool ImageInputType::shouldRespectHeightAndWidthAttributes()
{
    return true;
}

unsigned ImageInputType::height() const
{
    if (!element().layoutObject()) {
        // Check the attribute first for an explicit pixel value.
        unsigned height;
        if (parseHTMLNonNegativeInteger(element().fastGetAttribute(heightAttr), height))
            return height;

        // If the image is available, use its height.
        HTMLImageLoader* imageLoader = element().imageLoader();
        if (imageLoader && imageLoader->image())
            return imageLoader->image()->imageSize(LayoutObject::shouldRespectImageOrientation(nullptr), 1).height();
    }

    element().document().updateStyleAndLayout();

    LayoutBox* box = element().layoutBox();
    return box ? adjustForAbsoluteZoom(box->contentHeight(), box) : 0;
}

unsigned ImageInputType::width() const
{
    if (!element().layoutObject()) {
        // Check the attribute first for an explicit pixel value.
        unsigned width;
        if (parseHTMLNonNegativeInteger(element().fastGetAttribute(widthAttr), width))
            return width;

        // If the image is available, use its width.
        HTMLImageLoader* imageLoader = element().imageLoader();
        if (imageLoader && imageLoader->image())
            return imageLoader->image()->imageSize(LayoutObject::shouldRespectImageOrientation(nullptr), 1).width();
    }

    element().document().updateStyleAndLayout();

    LayoutBox* box = element().layoutBox();
    return box ? adjustForAbsoluteZoom(box->contentWidth(), box) : 0;
}

bool ImageInputType::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == srcAttr || BaseButtonInputType::hasLegalLinkAttribute(name);
}

const QualifiedName& ImageInputType::subResourceAttributeName() const
{
    return srcAttr;
}

void ImageInputType::ensureFallbackContent()
{
    if (m_useFallbackContent)
        return;
    setUseFallbackContent();
    reattachFallbackContent();
}

void ImageInputType::setUseFallbackContent()
{
    if (m_useFallbackContent)
        return;
    m_useFallbackContent = true;
    if (element().document().inStyleRecalc())
        return;
    if (ShadowRoot* root = element().userAgentShadowRoot())
        root->removeChildren();
    createShadowSubtree();
}

void ImageInputType::ensurePrimaryContent()
{
    if (!m_useFallbackContent)
        return;
    m_useFallbackContent = false;
    if (ShadowRoot* root = element().userAgentShadowRoot())
        root->removeChildren();
    createShadowSubtree();
    reattachFallbackContent();
}

void ImageInputType::reattachFallbackContent()
{
    // This can happen inside of attach() in the middle of a recalcStyle so we need to
    // reattach synchronously here.
    if (element().document().inStyleRecalc())
        element().reattach();
    else
        element().lazyReattachIfAttached();
}

void ImageInputType::createShadowSubtree()
{
    if (!m_useFallbackContent) {
        BaseButtonInputType::createShadowSubtree();
        return;
    }
    HTMLImageFallbackHelper::createAltTextShadowTree(element());
}

PassRefPtr<ComputedStyle> ImageInputType::customStyleForLayoutObject(PassRefPtr<ComputedStyle> newStyle)
{
    if (!m_useFallbackContent)
        return newStyle;

    return HTMLImageFallbackHelper::customStyleForAltText(element(), newStyle);
}

} // namespace blink
