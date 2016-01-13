/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLImageLoader.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/events/Event.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"

namespace WebCore {

HTMLImageLoader::HTMLImageLoader(Element* node)
    : ImageLoader(node)
{
}

HTMLImageLoader::~HTMLImageLoader()
{
}

void HTMLImageLoader::dispatchLoadEvent()
{
    // HTMLVideoElement uses this class to load the poster image, but it should not fire events for loading or failure.
    if (isHTMLVideoElement(*element()))
        return;

    bool errorOccurred = image()->errorOccurred();
    if (isHTMLObjectElement(*element()) && !errorOccurred)
        errorOccurred = (image()->response().httpStatusCode() >= 400); // An <object> considers a 404 to be an error and should fire onerror.
    element()->dispatchEvent(Event::create(errorOccurred ? EventTypeNames::error : EventTypeNames::load));
}

String HTMLImageLoader::sourceURI(const AtomicString& attr) const
{
    return stripLeadingAndTrailingHTMLSpaces(attr);
}

void HTMLImageLoader::notifyFinished(Resource*)
{
    ImageResource* cachedImage = image();

    RefPtrWillBeRawPtr<Element> element = this->element();
    ImageLoader::notifyFinished(cachedImage);

    bool loadError = cachedImage->errorOccurred() || cachedImage->response().httpStatusCode() >= 400;

    if (loadError && isHTMLObjectElement(*element))
        toHTMLObjectElement(element)->renderFallbackContent();
}

}
