/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
#include "core/html/HTMLAnchorElement.h"

#include "core/dom/Attribute.h"
#include "core/editing/FrameSelection.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/PingLoader.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/rendering/RenderImage.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/network/DNS.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPrescientNetworking.h"
#include "public/platform/WebURL.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

namespace {

void preconnectToURL(const KURL& url, blink::WebPreconnectMotivation motivation)
{
    blink::WebPrescientNetworking* prescientNetworking = blink::Platform::current()->prescientNetworking();
    if (!prescientNetworking)
        return;

    prescientNetworking->preconnect(url, motivation);
}

}

class HTMLAnchorElement::PrefetchEventHandler FINAL : public NoBaseWillBeGarbageCollected<HTMLAnchorElement::PrefetchEventHandler> {
public:
    static PassOwnPtrWillBeRawPtr<PrefetchEventHandler> create(HTMLAnchorElement* anchorElement)
    {
        return adoptPtrWillBeNoop(new HTMLAnchorElement::PrefetchEventHandler(anchorElement));
    }

    void reset();

    void handleEvent(Event* e);
    void didChangeHREF() { m_hadHREFChanged = true; }
    bool hasIssuedPreconnect() const { return m_hasIssuedPreconnect; }

    void trace(Visitor* visitor) { visitor->trace(m_anchorElement); }

private:
    explicit PrefetchEventHandler(HTMLAnchorElement*);

    void handleMouseOver(Event* event);
    void handleMouseOut(Event* event);
    void handleLeftMouseDown(Event* event);
    void handleGestureTapUnconfirmed(Event*);
    void handleGestureShowPress(Event*);
    void handleClick(Event* event);

    bool shouldPrefetch(const KURL&);
    void prefetch(blink::WebPreconnectMotivation);

    RawPtrWillBeMember<HTMLAnchorElement> m_anchorElement;
    double m_mouseOverTimestamp;
    double m_mouseDownTimestamp;
    double m_tapDownTimestamp;
    bool m_hadHREFChanged;
    bool m_hadTapUnconfirmed;
    bool m_hasIssuedPreconnect;
};

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_linkRelations(0)
    , m_cachedVisitedLinkHash(0)
{
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<HTMLAnchorElement> HTMLAnchorElement::create(Document& document)
{
    return adoptRefWillBeNoop(new HTMLAnchorElement(aTag, document));
}

HTMLAnchorElement::~HTMLAnchorElement()
{
}

bool HTMLAnchorElement::supportsFocus() const
{
    if (rendererIsEditable())
        return HTMLElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || HTMLElement::supportsFocus();
}

bool HTMLAnchorElement::isMouseFocusable() const
{
    // Links are focusable by default, but only allow links with tabindex or contenteditable to be mouse focusable.
    // https://bugs.webkit.org/show_bug.cgi?id=26856
    if (isLink())
        return HTMLElement::supportsFocus();

    return HTMLElement::isMouseFocusable();
}

bool HTMLAnchorElement::isKeyboardFocusable() const
{
    ASSERT(document().isActive());

    if (isFocusable() && Element::supportsFocus())
        return HTMLElement::isKeyboardFocusable();

    if (isLink() && !document().frameHost()->chrome().client().tabsToLinks())
        return false;
    return HTMLElement::isKeyboardFocusable();
}

static void appendServerMapMousePosition(StringBuilder& url, Event* event)
{
    if (!event->isMouseEvent())
        return;

    ASSERT(event->target());
    Node* target = event->target()->toNode();
    ASSERT(target);
    if (!isHTMLImageElement(*target))
        return;

    HTMLImageElement& imageElement = toHTMLImageElement(*target);
    if (!imageElement.isServerMap())
        return;

    if (!imageElement.renderer() || !imageElement.renderer()->isRenderImage())
        return;
    RenderImage* renderer = toRenderImage(imageElement.renderer());

    // FIXME: This should probably pass true for useTransforms.
    FloatPoint absolutePosition = renderer->absoluteToLocal(FloatPoint(toMouseEvent(event)->pageX(), toMouseEvent(event)->pageY()));
    int x = absolutePosition.x();
    int y = absolutePosition.y();
    url.append('?');
    url.appendNumber(x);
    url.append(',');
    url.appendNumber(y);
}

void HTMLAnchorElement::defaultEventHandler(Event* event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event) && isLiveLink()) {
            event->setDefaultHandled();
            dispatchSimulatedClick(event);
            return;
        }

        prefetchEventHandler()->handleEvent(event);

        if (isLinkClick(event) && isLiveLink()) {
            handleClick(event);
            prefetchEventHandler()->reset();
            return;
        }
    }

    HTMLElement::defaultEventHandler(event);
}

void HTMLAnchorElement::setActive(bool down)
{
    if (rendererIsEditable())
        return;

    ContainerNode::setActive(down);
}

void HTMLAnchorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull());
        if (wasLink != isLink()) {
            didAffectSelector(AffectedSelectorLink | AffectedSelectorVisited | AffectedSelectorEnabled);
            if (wasLink && treeScope().adjustedFocusedElement() == this) {
                // We might want to call blur(), but it's dangerous to dispatch
                // events here.
                document().setNeedsFocusedElementCheck();
            }
        }
        if (isLink()) {
            String parsedURL = stripLeadingAndTrailingHTMLSpaces(value);
            if (document().isDNSPrefetchEnabled()) {
                if (protocolIs(parsedURL, "http") || protocolIs(parsedURL, "https") || parsedURL.startsWith("//"))
                    prefetchDNS(document().completeURL(parsedURL).host());
            }

            if (wasLink)
                prefetchEventHandler()->didChangeHREF();
        }
        invalidateCachedVisitedLinkHash();
    } else if (name == nameAttr || name == titleAttr) {
        // Do nothing.
    } else if (name == relAttr)
        setRel(value);
    else
        HTMLElement::parseAttribute(name, value);
}

void HTMLAnchorElement::accessKeyAction(bool sendMouseEvents)
{
    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

bool HTMLAnchorElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLAnchorElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == hrefAttr || HTMLElement::hasLegalLinkAttribute(name);
}

bool HTMLAnchorElement::canStartSelection() const
{
    if (!isLink())
        return HTMLElement::canStartSelection();
    return rendererIsEditable();
}

bool HTMLAnchorElement::draggable() const
{
    // Should be draggable if we have an href attribute.
    const AtomicString& value = getAttribute(draggableAttr);
    if (equalIgnoringCase(value, "true"))
        return true;
    if (equalIgnoringCase(value, "false"))
        return false;
    return hasAttribute(hrefAttr);
}

KURL HTMLAnchorElement::href() const
{
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(hrefAttr)));
}

void HTMLAnchorElement::setHref(const AtomicString& value)
{
    setAttribute(hrefAttr, value);
}

KURL HTMLAnchorElement::url() const
{
    return href();
}

void HTMLAnchorElement::setURL(const KURL& url)
{
    setHref(AtomicString(url.string()));
}

String HTMLAnchorElement::input() const
{
    return getAttribute(hrefAttr);
}

void HTMLAnchorElement::setInput(const String& value)
{
    setHref(AtomicString(value));
}

bool HTMLAnchorElement::hasRel(uint32_t relation) const
{
    return m_linkRelations & relation;
}

void HTMLAnchorElement::setRel(const AtomicString& value)
{
    m_linkRelations = 0;
    SpaceSplitString newLinkRelations(value, true);
    // FIXME: Add link relations as they are implemented
    if (newLinkRelations.contains("noreferrer"))
        m_linkRelations |= RelationNoReferrer;
}

const AtomicString& HTMLAnchorElement::name() const
{
    return getNameAttribute();
}

short HTMLAnchorElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

AtomicString HTMLAnchorElement::target() const
{
    return getAttribute(targetAttr);
}

bool HTMLAnchorElement::isLiveLink() const
{
    return isLink() && !rendererIsEditable();
}

void HTMLAnchorElement::sendPings(const KURL& destinationURL)
{
    const AtomicString& pingValue = getAttribute(pingAttr);
    if (pingValue.isNull() || !document().settings() || !document().settings()->hyperlinkAuditingEnabled())
        return;

    UseCounter::count(document(), UseCounter::HTMLAnchorElementPingAttribute);

    SpaceSplitString pingURLs(pingValue, false);
    for (unsigned i = 0; i < pingURLs.size(); i++)
        PingLoader::sendLinkAuditPing(document().frame(), document().completeURL(pingURLs[i]), destinationURL);
}

void HTMLAnchorElement::handleClick(Event* event)
{
    event->setDefaultHandled();

    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    StringBuilder url;
    url.append(stripLeadingAndTrailingHTMLSpaces(fastGetAttribute(hrefAttr)));
    appendServerMapMousePosition(url, event);
    KURL completedURL = document().completeURL(url.toString());

    // Schedule the ping before the frame load. Prerender in Chrome may kill the renderer as soon as the navigation is
    // sent out.
    sendPings(completedURL);

    ResourceRequest request(completedURL);
    if (prefetchEventHandler()->hasIssuedPreconnect())
        frame->loader().client()->dispatchWillRequestAfterPreconnect(request);
    if (hasAttribute(downloadAttr)) {
        if (!hasRel(RelationNoReferrer)) {
            String referrer = SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), completedURL, document().outgoingReferrer());
            if (!referrer.isEmpty())
                request.setHTTPReferrer(Referrer(referrer, document().referrerPolicy()));
        }

        bool isSameOrigin = completedURL.protocolIsData() || document().securityOrigin()->canRequest(completedURL);
        const AtomicString& suggestedName = (isSameOrigin ? fastGetAttribute(downloadAttr) : nullAtom);

        frame->loader().client()->loadURLExternally(request, NavigationPolicyDownload, suggestedName);
    } else {
        FrameLoadRequest frameRequest(&document(), request, target());
        frameRequest.setTriggeringEvent(event);
        if (hasRel(RelationNoReferrer))
            frameRequest.setShouldSendReferrer(NeverSendReferrer);
        frame->loader().load(frameRequest);
    }
}

bool isEnterKeyKeydownEvent(Event* event)
{
    return event->type() == EventTypeNames::keydown && event->isKeyboardEvent() && toKeyboardEvent(event)->keyIdentifier() == "Enter";
}

bool isLinkClick(Event* event)
{
    return event->type() == EventTypeNames::click && (!event->isMouseEvent() || toMouseEvent(event)->button() != RightButton);
}

bool HTMLAnchorElement::willRespondToMouseClickEvents()
{
    return isLink() || HTMLElement::willRespondToMouseClickEvents();
}

HTMLAnchorElement::PrefetchEventHandler* HTMLAnchorElement::prefetchEventHandler()
{
    if (!m_prefetchEventHandler)
        m_prefetchEventHandler = PrefetchEventHandler::create(this);

    return m_prefetchEventHandler.get();
}

HTMLAnchorElement::PrefetchEventHandler::PrefetchEventHandler(HTMLAnchorElement* anchorElement)
    : m_anchorElement(anchorElement)
{
    ASSERT(m_anchorElement);

    reset();
}

void HTMLAnchorElement::PrefetchEventHandler::reset()
{
    m_hadHREFChanged = false;
    m_mouseOverTimestamp = 0;
    m_mouseDownTimestamp = 0;
    m_hadTapUnconfirmed = false;
    m_tapDownTimestamp = 0;
    m_hasIssuedPreconnect = false;
}

void HTMLAnchorElement::PrefetchEventHandler::handleEvent(Event* event)
{
    if (!shouldPrefetch(m_anchorElement->href()))
        return;

    if (event->type() == EventTypeNames::mouseover)
        handleMouseOver(event);
    else if (event->type() == EventTypeNames::mouseout)
        handleMouseOut(event);
    else if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton)
        handleLeftMouseDown(event);
    else if (event->type() == EventTypeNames::gestureshowpress)
        handleGestureShowPress(event);
    else if (event->type() == EventTypeNames::gesturetapunconfirmed)
        handleGestureTapUnconfirmed(event);
    else if (isLinkClick(event))
        handleClick(event);
}

void HTMLAnchorElement::PrefetchEventHandler::handleMouseOver(Event* event)
{
    if (m_mouseOverTimestamp == 0.0) {
        m_mouseOverTimestamp = event->timeStamp();

        blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.MouseOvers", 0, 2);

        prefetch(blink::WebPreconnectMotivationLinkMouseOver);
    }
}

void HTMLAnchorElement::PrefetchEventHandler::handleMouseOut(Event* event)
{
    if (m_mouseOverTimestamp > 0.0) {
        double mouseOverDuration = convertDOMTimeStampToSeconds(event->timeStamp() - m_mouseOverTimestamp);
        blink::Platform::current()->histogramCustomCounts("MouseEventPrefetch.MouseOverDuration_NoClick", mouseOverDuration * 1000, 0, 10000, 100);

        m_mouseOverTimestamp = 0.0;
    }
}

void HTMLAnchorElement::PrefetchEventHandler::handleLeftMouseDown(Event* event)
{
    m_mouseDownTimestamp = event->timeStamp();

    blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.MouseDowns", 0, 2);

    prefetch(blink::WebPreconnectMotivationLinkMouseDown);
}

void HTMLAnchorElement::PrefetchEventHandler::handleGestureTapUnconfirmed(Event* event)
{
    m_hadTapUnconfirmed = true;

    blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.TapUnconfirmeds", 0, 2);

    prefetch(blink::WebPreconnectMotivationLinkTapUnconfirmed);
}

void HTMLAnchorElement::PrefetchEventHandler::handleGestureShowPress(Event* event)
{
    m_tapDownTimestamp = event->timeStamp();

    blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.TapDowns", 0, 2);

    prefetch(blink::WebPreconnectMotivationLinkTapDown);
}

void HTMLAnchorElement::PrefetchEventHandler::handleClick(Event* event)
{
    bool capturedMouseOver = (m_mouseOverTimestamp > 0.0);
    if (capturedMouseOver) {
        double mouseOverDuration = convertDOMTimeStampToSeconds(event->timeStamp() - m_mouseOverTimestamp);

        blink::Platform::current()->histogramCustomCounts("MouseEventPrefetch.MouseOverDuration_Click", mouseOverDuration * 1000, 0, 10000, 100);
    }

    bool capturedMouseDown = (m_mouseDownTimestamp > 0.0);
    blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.MouseDownFollowedByClick", capturedMouseDown, 2);

    if (capturedMouseDown) {
        double mouseDownDuration = convertDOMTimeStampToSeconds(event->timeStamp() - m_mouseDownTimestamp);

        blink::Platform::current()->histogramCustomCounts("MouseEventPrefetch.MouseDownDuration_Click", mouseDownDuration * 1000, 0, 10000, 100);
    }

    bool capturedTapDown = (m_tapDownTimestamp > 0.0);
    if (capturedTapDown) {
        double tapDownDuration = convertDOMTimeStampToSeconds(event->timeStamp() - m_tapDownTimestamp);

        blink::Platform::current()->histogramCustomCounts("MouseEventPrefetch.TapDownDuration_Click", tapDownDuration * 1000, 0, 10000, 100);
    }

    int flags = (m_hadTapUnconfirmed ? 2 : 0) | (capturedTapDown ? 1 : 0);
    blink::Platform::current()->histogramEnumeration("MouseEventPrefetch.PreTapEventsFollowedByClick", flags, 4);
}

bool HTMLAnchorElement::PrefetchEventHandler::shouldPrefetch(const KURL& url)
{
    if (m_hadHREFChanged)
        return false;

    if (m_anchorElement->hasEventListeners(EventTypeNames::click))
        return false;

    if (!url.protocolIsInHTTPFamily())
        return false;

    Document& document = m_anchorElement->document();

    if (!document.securityOrigin()->canDisplay(url))
        return false;

    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(document.url(), url))
        return false;

    LocalFrame* frame = document.frame();
    if (!frame)
        return false;

    // Links which create new window/tab are avoided because they may require user approval interaction.
    if (!m_anchorElement->target().isEmpty())
        return false;

    return true;
}

void HTMLAnchorElement::PrefetchEventHandler::prefetch(blink::WebPreconnectMotivation motivation)
{
    const KURL& url = m_anchorElement->href();

    if (!shouldPrefetch(url))
        return;

    // The precision of current MouseOver trigger is too low to actually trigger preconnects.
    if (motivation == blink::WebPreconnectMotivationLinkMouseOver)
        return;

    preconnectToURL(url, motivation);
    m_hasIssuedPreconnect = true;
}

bool HTMLAnchorElement::isInteractiveContent() const
{
    return isLink();
}

void HTMLAnchorElement::trace(Visitor* visitor)
{
    visitor->trace(m_prefetchEventHandler);
    HTMLElement::trace(visitor);
}

}
