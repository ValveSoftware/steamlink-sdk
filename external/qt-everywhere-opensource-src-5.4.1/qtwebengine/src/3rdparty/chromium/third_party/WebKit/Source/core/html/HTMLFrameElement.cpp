/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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
#include "core/html/HTMLFrameElement.h"

#include "core/HTMLNames.h"
#include "core/html/HTMLFrameSetElement.h"
#include "core/rendering/RenderFrame.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLFrameElement::HTMLFrameElement(Document& document)
    : HTMLFrameElementBase(frameTag, document)
    , m_frameBorder(true)
    , m_frameBorderSet(false)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(HTMLFrameElement)

bool HTMLFrameElement::rendererIsNeeded(const RenderStyle&)
{
    // For compatibility, frames render even when display: none is set.
    return isURLAllowed();
}

RenderObject* HTMLFrameElement::createRenderer(RenderStyle*)
{
    return new RenderFrame(this);
}

bool HTMLFrameElement::noResize() const
{
    return hasAttribute(noresizeAttr);
}

void HTMLFrameElement::attach(const AttachContext& context)
{
    HTMLFrameElementBase::attach(context);

    if (HTMLFrameSetElement* frameSetElement = Traversal<HTMLFrameSetElement>::firstAncestor(*this)) {
        if (!m_frameBorderSet)
            m_frameBorder = frameSetElement->hasFrameBorder();
    }
}

void HTMLFrameElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == frameborderAttr) {
        m_frameBorder = value.toInt();
        m_frameBorderSet = !value.isNull();
        // FIXME: If we are already attached, this has no effect.
    } else if (name == noresizeAttr) {
        if (renderer())
            renderer()->updateFromElement();
    } else
        HTMLFrameElementBase::parseAttribute(name, value);
}

} // namespace WebCore
