/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
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
#include "core/html/HTMLIFrameElement.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/html/HTMLDocument.h"
#include "core/rendering/RenderIFrame.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(Document& document)
    : HTMLFrameElementBase(iframeTag, document)
    , m_didLoadNonEmptyDocument(false)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(HTMLIFrameElement)

bool HTMLIFrameElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == alignAttr || name == frameborderAttr)
        return true;
    return HTMLFrameElementBase::isPresentationAttribute(name);
}

void HTMLIFrameElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == frameborderAttr) {
        // LocalFrame border doesn't really match the HTML4 spec definition for iframes. It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!value.toInt()) {
            // Add a rule that nulls out our border width.
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else
        HTMLFrameElementBase::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLIFrameElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == nameAttr) {
        if (inDocument() && document().isHTMLDocument() && !isInShadowTree()) {
            HTMLDocument& document = toHTMLDocument(this->document());
            document.removeExtraNamedItem(m_name);
            document.addExtraNamedItem(value);
        }
        m_name = value;
    } else if (name == sandboxAttr) {
        String invalidTokens;
        setSandboxFlags(value.isNull() ? SandboxNone : parseSandboxPolicy(value, invalidTokens));
        if (!invalidTokens.isNull())
            document().addConsoleMessage(OtherMessageSource, ErrorMessageLevel, "Error while parsing the 'sandbox' attribute: " + invalidTokens);
    } else {
        HTMLFrameElementBase::parseAttribute(name, value);
    }
}

bool HTMLIFrameElement::rendererIsNeeded(const RenderStyle& style)
{
    return isURLAllowed() && HTMLElement::rendererIsNeeded(style);
}

RenderObject* HTMLIFrameElement::createRenderer(RenderStyle*)
{
    return new RenderIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::insertedInto(ContainerNode* insertionPoint)
{
    InsertionNotificationRequest result = HTMLFrameElementBase::insertedInto(insertionPoint);
    if (insertionPoint->inDocument() && document().isHTMLDocument() && !insertionPoint->isInShadowTree())
        toHTMLDocument(document()).addExtraNamedItem(m_name);
    return result;
}

void HTMLIFrameElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLFrameElementBase::removedFrom(insertionPoint);
    if (insertionPoint->inDocument() && document().isHTMLDocument() && !insertionPoint->isInShadowTree())
        toHTMLDocument(document()).removeExtraNamedItem(m_name);
}

bool HTMLIFrameElement::isInteractiveContent() const
{
    return true;
}

}
