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

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName,
                                           Document& document)
    : HTMLFrameOwnerElement(tagName, document),
      m_scrollingMode(ScrollbarAuto),
      m_marginWidth(-1),
      m_marginHeight(-1) {}

bool HTMLFrameElementBase::isURLAllowed() const {
  if (m_URL.isEmpty())
    return true;

  const KURL& completeURL = document().completeURL(m_URL);

  if (contentFrame() && protocolIsJavaScript(completeURL)) {
    // Check if the caller can execute script in the context of the content
    // frame. NB: This check can be invoked without any JS on the stack for some
    // parser operations. In such case, we use the origin of the frame element's
    // containing document as the caller context.
    v8::Isolate* isolate = toIsolate(&document());
    LocalDOMWindow* accessingWindow = isolate->InContext()
                                          ? currentDOMWindow(isolate)
                                          : document().domWindow();
    if (!BindingSecurity::shouldAllowAccessToFrame(
            accessingWindow, contentFrame(),
            BindingSecurity::ErrorReportOption::Report))
      return false;
  }

  LocalFrame* parentFrame = document().frame();
  if (parentFrame)
    return parentFrame->isURLAllowed(completeURL);

  return true;
}

void HTMLFrameElementBase::openURL(bool replaceCurrentItem) {
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
    // We'll set/execute |scriptURL| iff CSP allows us to execute inline
    // JavaScript. If CSP blocks inline JavaScript, then exit early if
    // we're trying to execute script in an existing document. If we're
    // executing JavaScript to create a new document (e.g.
    // '<iframe src="javascript:...">' then continue loading 'about:blank'
    // so that the frame is populated with something reasonable.
    if (ContentSecurityPolicy::shouldBypassMainWorld(&document()) ||
        document().contentSecurityPolicy()->allowJavaScriptURLs(
            this, document().url(), OrdinalNumber::first())) {
      scriptURL = url;
    } else {
      if (contentFrame())
        return;
    }

    url = blankURL();
  }

  if (!loadOrRedirectSubframe(url, m_frameName, replaceCurrentItem))
    return;
  if (!contentFrame() || scriptURL.isEmpty() || !contentFrame()->isLocalFrame())
    return;
  if (contentFrame()->owner()->getSandboxFlags() & SandboxOrigin)
    return;
  toLocalFrame(contentFrame())
      ->script()
      .executeScriptIfJavaScriptURL(scriptURL, this);
}

void HTMLFrameElementBase::frameOwnerPropertiesChanged() {
  // Don't notify about updates if contentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (contentFrame())
    document().frame()->loader().client()->didChangeFrameOwnerProperties(this);
}

void HTMLFrameElementBase::parseAttribute(const QualifiedName& name,
                                          const AtomicString& oldValue,
                                          const AtomicString& value) {
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
    // Important to call through to base for the id attribute so the hasID bit
    // gets set.
    HTMLFrameOwnerElement::parseAttribute(name, oldValue, value);
    m_frameName = value;
  } else if (name == nameAttr) {
    m_frameName = value;
    // FIXME: If we are already attached, this doesn't actually change the
    // frame's name.
    // FIXME: If we are already attached, this doesn't check for frame name
    // conflicts and generate a unique frame name.
  } else if (name == marginwidthAttr) {
    setMarginWidth(value.toInt());
    // FIXME: If we are already attached, this has no effect.
  } else if (name == marginheightAttr) {
    setMarginHeight(value.toInt());
    // FIXME: If we are already attached, this has no effect.
  } else if (name == scrollingAttr) {
    // Auto and yes both simply mean "allow scrolling." No means "don't allow
    // scrolling."
    if (equalIgnoringCase(value, "auto") || equalIgnoringCase(value, "yes"))
      setScrollingMode(ScrollbarAuto);
    else if (equalIgnoringCase(value, "no"))
      setScrollingMode(ScrollbarAlwaysOff);
    // FIXME: If we are already attached, this has no effect.
  } else if (name == onbeforeunloadAttr) {
    // FIXME: should <frame> elements have beforeunload handlers?
    setAttributeEventListener(
        EventTypeNames::beforeunload,
        createAttributeEventListener(this, name, value, eventParameterName()));
  } else {
    HTMLFrameOwnerElement::parseAttribute(name, oldValue, value);
  }
}

void HTMLFrameElementBase::setNameAndOpenURL() {
  m_frameName = getNameAttribute();
  openURL();
}

Node::InsertionNotificationRequest HTMLFrameElementBase::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLFrameOwnerElement::insertedInto(insertionPoint);
  return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFrameElementBase::didNotifySubtreeInsertionsToDocument() {
  if (!document().frame())
    return;

  if (!SubframeLoadingDisabler::canLoadFrame(*this))
    return;

  setNameAndOpenURL();
}

void HTMLFrameElementBase::attachLayoutTree(const AttachContext& context) {
  HTMLFrameOwnerElement::attachLayoutTree(context);

  if (layoutPart()) {
    if (Frame* frame = contentFrame()) {
      if (frame->isLocalFrame())
        setWidget(toLocalFrame(frame)->view());
      else if (frame->isRemoteFrame())
        setWidget(toRemoteFrame(frame)->view());
    }
  }
}

void HTMLFrameElementBase::setLocation(const String& str) {
  m_URL = AtomicString(str);

  if (isConnected())
    openURL(false);
}

bool HTMLFrameElementBase::supportsFocus() const {
  return true;
}

void HTMLFrameElementBase::setFocused(bool received) {
  HTMLFrameOwnerElement::setFocused(received);
  if (Page* page = document().page()) {
    if (received) {
      page->focusController().setFocusedFrame(contentFrame());
    } else if (page->focusController().focusedFrame() == contentFrame()) {
      // Focus may have already been given to another frame, don't take it away.
      page->focusController().setFocusedFrame(nullptr);
    }
  }
}

bool HTMLFrameElementBase::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == longdescAttr || attribute.name() == srcAttr ||
         HTMLFrameOwnerElement::isURLAttribute(attribute);
}

bool HTMLFrameElementBase::hasLegalLinkAttribute(
    const QualifiedName& name) const {
  return name == srcAttr || HTMLFrameOwnerElement::hasLegalLinkAttribute(name);
}

bool HTMLFrameElementBase::isHTMLContentAttribute(
    const Attribute& attribute) const {
  return attribute.name() == srcdocAttr ||
         HTMLFrameOwnerElement::isHTMLContentAttribute(attribute);
}

// FIXME: Remove this code once we have input routing in the browser
// process. See http://crbug.com/339659.
void HTMLFrameElementBase::defaultEventHandler(Event* event) {
  if (contentFrame() && contentFrame()->isRemoteFrame()) {
    toRemoteFrame(contentFrame())->forwardInputEvent(event);
    return;
  }
  HTMLFrameOwnerElement::defaultEventHandler(event);
}

void HTMLFrameElementBase::setScrollingMode(ScrollbarMode scrollbarMode) {
  m_scrollingMode = scrollbarMode;
  frameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::setMarginWidth(int marginWidth) {
  m_marginWidth = marginWidth;
  frameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::setMarginHeight(int marginHeight) {
  m_marginHeight = marginHeight;
  frameOwnerPropertiesChanged();
}

}  // namespace blink
