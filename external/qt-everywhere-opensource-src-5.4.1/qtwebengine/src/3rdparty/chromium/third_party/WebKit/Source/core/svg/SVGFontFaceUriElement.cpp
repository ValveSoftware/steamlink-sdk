/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontFaceUriElement.h"

#include "core/XLinkNames.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/svg/SVGFontFaceElement.h"

namespace WebCore {

using namespace SVGNames;

inline SVGFontFaceUriElement::SVGFontFaceUriElement(Document& document)
    : SVGElement(font_face_uriTag, document)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGFontFaceUriElement)

SVGFontFaceUriElement::~SVGFontFaceUriElement()
{
    if (m_resource)
        m_resource->removeClient(this);
}

PassRefPtrWillBeRawPtr<CSSFontFaceSrcValue> SVGFontFaceUriElement::srcValue() const
{
    RefPtrWillBeRawPtr<CSSFontFaceSrcValue> src = CSSFontFaceSrcValue::create(getAttribute(XLinkNames::hrefAttr));
    AtomicString value(fastGetAttribute(formatAttr));
    src->setFormat(value.isEmpty() ? "svg" : value); // Default format
    return src.release();
}

void SVGFontFaceUriElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name.matches(XLinkNames::hrefAttr))
        loadFont();
    else
        SVGElement::parseAttribute(name, value);
}

void SVGFontFaceUriElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!isSVGFontFaceSrcElement(parentNode()))
        return;

    ContainerNode* grandparent = parentNode()->parentNode();
    if (isSVGFontFaceElement(grandparent))
        toSVGFontFaceElement(*grandparent).rebuildFontFace();
}

Node::InsertionNotificationRequest SVGFontFaceUriElement::insertedInto(ContainerNode* rootParent)
{
    loadFont();
    return SVGElement::insertedInto(rootParent);
}

void SVGFontFaceUriElement::loadFont()
{
    if (m_resource)
        m_resource->removeClient(this);

    const AtomicString& href = getAttribute(XLinkNames::hrefAttr);
    if (!href.isNull()) {
        ResourceFetcher* fetcher = document().fetcher();
        FetchRequest request(ResourceRequest(document().completeURL(href)), localName());
        m_resource = fetcher->fetchFont(request);
        if (m_resource) {
            m_resource->addClient(this);
            m_resource->beginLoadIfNeeded(fetcher);
        }
    } else {
        m_resource = 0;
    }
}

}

#endif // ENABLE(SVG_FONTS)
