/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "core/html/HTMLFrameElementBase.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName, Document& document)
    : HTMLFrameOwnerElement(tagName, document)
    , m_scrollingMode(ScrollbarAuto)
    , m_marginWidth(-1)
    , m_marginHeight(-1)
{
}

bool HTMLFrameElementBase::isURLAllowed() const
{
    if (m_URL.isEmpty())
        return true;

    const KURL& completeURL = document().completeURL(m_URL);

    if (protocolIsJavaScript(completeURL)) {
        if (contentFrame() && !ScriptController::canAccessFromCurrentOrigin(toIsolate(&document()), contentFrame()))
            return false;
    }

    LocalFrame* parentFrame = document().frame();
    if (parentFrame)
        return parentFrame->isURLAllowed(completeURL);

    return true;
}

void HTMLFrameElementBase::openURL(bool replaceCurrentItem)
{
    if (!isURLAllowed())
        return;

    if (m_URL.isEmpty())
        m_URL = AtomicString(blankURL().getString());

    LocalFrame* parentFrame = document().frame();
    if (!parentFrame)
        return;

    // Support for <frame src="javascript:string">
    KURL scriptURL;
    KURL url = document().completeURL(m_URL);
    if (protocolIsJavaScript(m_URL)) {
        scriptURL = url;
        url = blankURL();
    }

    if (!loadOrRedirectSubframe(url, m_frameName, replaceCurrentItem))
        return;
    if (!contentFrame() || scriptURL.isEmpty() || !contentFrame()->isLocalFrame())
        return;
    toLocalFrame(contentFrame())->script().executeScriptIfJavaScriptURL(scriptURL);
}

void HTMLFrameElementBase::frameOwnerPropertiesChanged()
{
    // Don't notify about updates if contentFrame() is null, for example when
    // the subframe hasn't been created yet.
    if (contentFrame())
        document().frame()->loader().client()->didChangeFrameOwnerProperties(this);
}

void HTMLFrameElementBase::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == srcdocAttr) {
        if (!value.isNull()) {
            setLocation(srcdocURL().getString());
        } else {
            const AtomicString& srcValue = fastGetAttribute(srcAttr);
            if (!srcValue.isNull())
                setLocation(stripLeadingAndTrailingHTMLSpaces(srcValue));
        }
    } else if (name == srcAttr && !fastHasAttribute(srcdocAttr)) {
        setLocation(stripLeadingAndTrailingHTMLSpaces(value));
    } else if (name == idAttr) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLFrameOwnerElement::parseAttribute(name, oldValue, value);
        m_frameName = value;
    } else if (name == nameAttr) {
        m_frameName = value;
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
    } else if (name == marginwidthAttr) {
        setMarginWidth(value.toInt());
        // FIXME: If we are already attached, this has no effect.
    } else if (name == marginheightAttr) {
        setMarginHeight(value.toInt());
        // FIXME: If we are already attached, this has no effect.
    } else if (name == scrollingAttr) {
        // Auto and yes both simply mean "allow scrolling." No means "don't allow scrolling."
        if (equalIgnoringCase(value, "auto") || equalIgnoringCase(value, "yes"))
            setScrollingMode(ScrollbarAuto);
        else if (equalIgnoringCase(value, "no"))
            setScrollingMode(ScrollbarAlwaysOff);
        // FIXME: If we are already attached, this has no effect.
    } else if (name == onbeforeunloadAttr) {
        // FIXME: should <frame> elements have beforeunload handlers?
        setAttributeEventListener(EventTypeNames::beforeunload, createAttributeEventListener(this, name, value, eventParameterName()));
    } else {
        HTMLFrameOwnerElement::parseAttribute(name, oldValue, value);
    }
}

void HTMLFrameElementBase::setNameAndOpenURL()
{
    m_frameName = getNameAttribute();
    openURL();
}

Node::InsertionNotificationRequest HTMLFrameElementBase::insertedInto(ContainerNode* insertionPoint)
{
    HTMLFrameOwnerElement::insertedInto(insertionPoint);
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFrameElementBase::didNotifySubtreeInsertionsToDocument()
{
    if (!document().frame())
        return;

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return;

    setNameAndOpenURL();
}

void HTMLFrameElementBase::attach(const AttachContext& context)
{
    HTMLFrameOwnerElement::attach(context);

    if (layoutPart()) {
        if (Frame* frame = contentFrame()) {
            if (frame->isLocalFrame())
                setWidget(toLocalFrame(frame)->view());
            else if (frame->isRemoteFrame())
                setWidget(toRemoteFrame(frame)->view());
        }
    }
}

void HTMLFrameElementBase::setLocation(const String& str)
{
    m_URL = AtomicString(str);

    if (inShadowIncludingDocument())
        openURL(false);
}

bool HTMLFrameElementBase::supportsFocus() const
{
    return true;
}

void HTMLFrameElementBase::setFocus(bool received)
{
    HTMLFrameOwnerElement::setFocus(received);
    if (Page* page = document().page()) {
        if (received)
            page->focusController().setFocusedFrame(contentFrame());
        else if (page->focusController().focusedFrame() == contentFrame()) // Focus may have already been given to another frame, don't take it away.
            page->focusController().setFocusedFrame(nullptr);
    }
}

bool HTMLFrameElementBase::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == longdescAttr || attribute.name() == srcAttr
        || HTMLFrameOwnerElement::isURLAttribute(attribute);
}

bool HTMLFrameElementBase::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == srcAttr || HTMLFrameOwnerElement::hasLegalLinkAttribute(name);
}

bool HTMLFrameElementBase::isHTMLContentAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcdocAttr || HTMLFrameOwnerElement::isHTMLContentAttribute(attribute);
}

// FIXME: Remove this code once we have input routing in the browser
// process. See http://crbug.com/339659.
void HTMLFrameElementBase::defaultEventHandler(Event* event)
{
    if (contentFrame() && contentFrame()->isRemoteFrame()) {
        toRemoteFrame(contentFrame())->forwardInputEvent(event);
        return;
    }
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

void HTMLFrameElementBase::setScrollingMode(ScrollbarMode scrollbarMode)
{
    m_scrollingMode = scrollbarMode;
    frameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::setMarginWidth(int marginWidth)
{
    m_marginWidth = marginWidth;
    frameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::setMarginHeight(int marginHeight)
{
    m_marginHeight = marginHeight;
    frameOwnerPropertiesChanged();
}

} // namespace blink
