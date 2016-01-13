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

#include "config.h"
#include "core/html/forms/ImageInputType.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/events/MouseEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderImage.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

inline ImageInputType::ImageInputType(HTMLInputElement& element)
    : BaseButtonInputType(element)
{
}

PassRefPtrWillBeRawPtr<InputType> ImageInputType::create(HTMLInputElement& element)
{
    return adoptRefWillBeNoop(new ImageInputType(element));
}

const AtomicString& ImageInputType::formControlType() const
{
    return InputTypeNames::image;
}

bool ImageInputType::isFormDataAppendable() const
{
    return true;
}

bool ImageInputType::appendFormData(FormDataList& encoding, bool) const
{
    if (!element().isActivatedSubmit())
        return false;
    const AtomicString& name = element().name();
    if (name.isEmpty()) {
        encoding.appendData("x", m_clickLocation.x());
        encoding.appendData("y", m_clickLocation.y());
        return true;
    }

    DEFINE_STATIC_LOCAL(String, dotXString, (".x"));
    DEFINE_STATIC_LOCAL(String, dotYString, (".y"));
    encoding.appendData(name + dotXString, m_clickLocation.x());
    encoding.appendData(name + dotYString, m_clickLocation.y());

    if (!element().value().isEmpty())
        encoding.appendData(name, element().value());
    return true;
}

String ImageInputType::resultForDialogSubmit() const
{
    StringBuilder result;
    result.appendNumber(m_clickLocation.x());
    result.append(",");
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
    if (mouseEvent->isSimulated())
        return IntPoint();
    return IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY());
}

void ImageInputType::handleDOMActivateEvent(Event* event)
{
    RefPtrWillBeRawPtr<HTMLInputElement> element(this->element());
    if (element->isDisabledFormControl() || !element->form())
        return;
    element->setActivatedSubmit(true);
    m_clickLocation = extractClickLocation(event);
    element->form()->prepareForSubmission(event); // Event handlers can run.
    element->setActivatedSubmit(false);
    event->setDefaultHandled();
}

RenderObject* ImageInputType::createRenderer(RenderStyle*) const
{
    RenderImage* image = new RenderImage(&element());
    image->setImageResource(RenderImageResource::create());
    return image;
}

void ImageInputType::altAttributeChanged()
{
    RenderImage* image = toRenderImage(element().renderer());
    if (!image)
        return;
    image->updateAltText();
}

void ImageInputType::srcAttributeChanged()
{
    if (!element().renderer())
        return;
    element().imageLoader()->updateFromElementIgnoringPreviousError();
}

void ImageInputType::startResourceLoading()
{
    BaseButtonInputType::startResourceLoading();

    HTMLImageLoader* imageLoader = element().imageLoader();
    imageLoader->updateFromElement();

    RenderImage* renderer = toRenderImage(element().renderer());
    if (!renderer)
        return;

    RenderImageResource* imageResource = renderer->imageResource();
    imageResource->setImageResource(imageLoader->image());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!imageLoader->image() && !imageResource->cachedImage())
        renderer->setImageSizeForAltText();
}

bool ImageInputType::shouldRespectAlignAttribute()
{
    return true;
}

bool ImageInputType::canBeSuccessfulSubmitButton()
{
    return true;
}

bool ImageInputType::isImageButton() const
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
    RefPtrWillBeRawPtr<HTMLInputElement> element(this->element());

    if (!element->renderer()) {
        // Check the attribute first for an explicit pixel value.
        unsigned height;
        if (parseHTMLNonNegativeInteger(element->fastGetAttribute(heightAttr), height))
            return height;

        // If the image is available, use its height.
        if (element->hasImageLoader()) {
            HTMLImageLoader* imageLoader = element->imageLoader();
            if (imageLoader->image())
                return imageLoader->image()->imageSizeForRenderer(element->renderer(), 1).height();
        }
    }

    element->document().updateLayout();

    RenderBox* box = element->renderBox();
    return box ? adjustForAbsoluteZoom(box->contentHeight(), box) : 0;
}

unsigned ImageInputType::width() const
{
    RefPtrWillBeRawPtr<HTMLInputElement> element(this->element());

    if (!element->renderer()) {
        // Check the attribute first for an explicit pixel value.
        unsigned width;
        if (parseHTMLNonNegativeInteger(element->fastGetAttribute(widthAttr), width))
            return width;

        // If the image is available, use its width.
        if (element->hasImageLoader()) {
            HTMLImageLoader* imageLoader = element->imageLoader();
            if (imageLoader->image())
                return imageLoader->image()->imageSizeForRenderer(element->renderer(), 1).width();
        }
    }

    element->document().updateLayout();

    RenderBox* box = element->renderBox();
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

} // namespace WebCore
