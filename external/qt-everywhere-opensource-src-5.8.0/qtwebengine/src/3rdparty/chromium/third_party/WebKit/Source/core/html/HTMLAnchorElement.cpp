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

#include "core/html/HTMLAnchorElement.h"

#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBox.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/PingLoader.h"
#include "core/page/ChromeClient.h"
#include "platform/network/NetworkHints.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

using namespace HTMLNames;

class HTMLAnchorElement::NavigationHintSender : public GarbageCollected<HTMLAnchorElement::NavigationHintSender> {
public:
    // TODO(horo): Move WebNavigationHintType to public/ directory.
    enum class WebNavigationHintType {
        Unknown,
        LinkMouseDown,
        LinkTapUnconfirmed,
        LinkTapDown,
        Last = LinkTapDown
    };

    static NavigationHintSender* create(HTMLAnchorElement* anchorElement)
    {
        return new NavigationHintSender(anchorElement);
    }
    void handleEvent(Event*);

    DECLARE_TRACE();

private:
    explicit NavigationHintSender(HTMLAnchorElement*);
    bool shouldSendNavigationHint() const;
    void maybeSendNavigationHint(WebNavigationHintType);

    Member<HTMLAnchorElement> m_anchorElement;
};

void HTMLAnchorElement::NavigationHintSender::handleEvent(Event* event)
{
    if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton)
        maybeSendNavigationHint(WebNavigationHintType::LinkMouseDown);
    else if (event->type() == EventTypeNames::gesturetapunconfirmed)
        maybeSendNavigationHint(WebNavigationHintType::LinkTapUnconfirmed);
    else if (event->type() == EventTypeNames::gestureshowpress)
        maybeSendNavigationHint(WebNavigationHintType::LinkTapDown);
}

DEFINE_TRACE(HTMLAnchorElement::NavigationHintSender)
{
    visitor->trace(m_anchorElement);
}

HTMLAnchorElement::NavigationHintSender::NavigationHintSender(HTMLAnchorElement* anchorElement)
    : m_anchorElement(anchorElement)
{
}

bool HTMLAnchorElement::NavigationHintSender::shouldSendNavigationHint() const
{
    const KURL& url = m_anchorElement->href();
    // Currently the navigation hint only supports HTTP and HTTPS.
    if (!url.protocolIsInHTTPFamily())
        return false;

    Document& document = m_anchorElement->document();
    // If the element was detached from the frame, handleClick() doesn't cause
    // the navigation.
    if (!document.frame())
        return false;

    // When the user clicks a link which is to the current document with a hash,
    // the network request is not fetched. So we don't send the navigation hint
    // to the browser process.
    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(document.url(), url))
        return false;

    return true;
}

void HTMLAnchorElement::NavigationHintSender::maybeSendNavigationHint(WebNavigationHintType type)
{
    if (!shouldSendNavigationHint())
        return;

    // TODO(horo): Send the navigation hint message to the browser process.
}

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_linkRelations(0)
    , m_cachedVisitedLinkHash(0)
    , m_wasFocusedByMouse(false)
{
}

HTMLAnchorElement* HTMLAnchorElement::create(Document& document)
{
    return new HTMLAnchorElement(aTag, document);
}

HTMLAnchorElement::~HTMLAnchorElement()
{
}

DEFINE_TRACE(HTMLAnchorElement)
{
    visitor->trace(m_navigationHintSender);
    HTMLElement::trace(visitor);
}

bool HTMLAnchorElement::supportsFocus() const
{
    if (hasEditableStyle())
        return HTMLElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || HTMLElement::supportsFocus();
}

bool HTMLAnchorElement::matchesEnabledPseudoClass() const
{
    return isLink();
}

bool HTMLAnchorElement::shouldHaveFocusAppearance() const
{
    return !m_wasFocusedByMouse || HTMLElement::supportsFocus();
}

void HTMLAnchorElement::dispatchFocusEvent(Element* oldFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    if (type != WebFocusTypePage)
        m_wasFocusedByMouse = type == WebFocusTypeMouse;
    HTMLElement::dispatchFocusEvent(oldFocusedElement, type, sourceCapabilities);
}

void HTMLAnchorElement::dispatchBlurEvent(Element* newFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    if (type != WebFocusTypePage)
        m_wasFocusedByMouse = false;
    HTMLElement::dispatchBlurEvent(newFocusedElement, type, sourceCapabilities);
}

bool HTMLAnchorElement::isMouseFocusable() const
{
    if (isLink())
        return supportsFocus();

    return HTMLElement::isMouseFocusable();
}

bool HTMLAnchorElement::isKeyboardFocusable() const
{
    ASSERT(document().isActive());

    if (isFocusable() && Element::supportsFocus())
        return HTMLElement::isKeyboardFocusable();

    if (isLink() && !document().frameHost()->chromeClient().tabsToLinks())
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

    LayoutObject* layoutObject = imageElement.layoutObject();
    if (!layoutObject || !layoutObject->isBox())
        return;

    // The coordinates sent in the query string are relative to the height and
    // width of the image element, ignoring CSS transform/zoom.
    LayoutPoint mapPoint(layoutObject->absoluteToLocal(FloatPoint(toMouseEvent(event)->absoluteLocation()), UseTransforms));

    // The origin (0,0) is at the upper left of the content area, inside the
    // padding and border.
    mapPoint -= toLayoutBox(layoutObject)->contentBoxOffset();

    // CSS zoom is not reflected in the map coordinates.
    float scaleFactor = 1 / layoutObject->style()->effectiveZoom();
    mapPoint.scale(scaleFactor, scaleFactor);

    // Negative coordinates are clamped to 0 such that clicks in the left and
    // top padding/border areas receive an X or Y coordinate of 0.
    IntPoint clampedPoint(roundedIntPoint(mapPoint));
    clampedPoint.clampNegativeToZero();

    url.append('?');
    url.appendNumber(clampedPoint.x());
    url.append(',');
    url.appendNumber(clampedPoint.y());
}

void HTMLAnchorElement::defaultEventHandler(Event* event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event) && isLiveLink()) {
            event->setDefaultHandled();
            dispatchSimulatedClick(event);
            return;
        }

        // TODO(horo): Call NavigationHintSender::handleEvent() when
        // SpeculativeLaunchServiceWorker feature is enabled.
        // ensureNavigationHintSender()->handleEvent(event);

        if (isLinkClick(event) && isLiveLink()) {
            handleClick(event);
            return;
        }
    }

    HTMLElement::defaultEventHandler(event);
}

void HTMLAnchorElement::setActive(bool down)
{
    if (hasEditableStyle())
        return;

    ContainerNode::setActive(down);
}

void HTMLAnchorElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull());
        if (wasLink || isLink()) {
            pseudoStateChanged(CSSSelector::PseudoLink);
            pseudoStateChanged(CSSSelector::PseudoVisited);
            pseudoStateChanged(CSSSelector::PseudoAnyLink);
        }
        if (wasLink && !isLink() && adjustedFocusedElementInTreeScope() == this) {
            // We might want to call blur(), but it's dangerous to dispatch
            // events here.
            document().setNeedsFocusedElementCheck();
        }
        if (isLink()) {
            String parsedURL = stripLeadingAndTrailingHTMLSpaces(value);
            if (document().isDNSPrefetchEnabled()) {
                if (protocolIs(parsedURL, "http") || protocolIs(parsedURL, "https") || parsedURL.startsWith("//"))
                    prefetchDNS(document().completeURL(parsedURL).host());
            }
        }
        invalidateCachedVisitedLinkHash();
        logUpdateAttributeIfIsolatedWorldAndInDocument("a", hrefAttr, oldValue, value);
    } else if (name == nameAttr || name == titleAttr) {
        // Do nothing.
    } else if (name == relAttr) {
        setRel(value);
    } else {
        HTMLElement::parseAttribute(name, oldValue, value);
    }
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
    return hasEditableStyle();
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
    setHref(AtomicString(url.getString()));
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
    SpaceSplitString newLinkRelations(value, SpaceSplitString::ShouldFoldCase);
    // FIXME: Add link relations as they are implemented
    if (newLinkRelations.contains("noreferrer"))
        m_linkRelations |= RelationNoReferrer;
    if (newLinkRelations.contains("noopener"))
        m_linkRelations |= RelationNoOpener;
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

bool HTMLAnchorElement::isLiveLink() const
{
    return isLink() && !hasEditableStyle();
}

void HTMLAnchorElement::sendPings(const KURL& destinationURL) const
{
    const AtomicString& pingValue = getAttribute(pingAttr);
    if (pingValue.isNull() || !document().settings() || !document().settings()->hyperlinkAuditingEnabled())
        return;

    UseCounter::count(document(), UseCounter::HTMLAnchorElementPingAttribute);

    SpaceSplitString pingURLs(pingValue, SpaceSplitString::ShouldNotFoldCase);
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
    request.setUIStartTime(event->platformTimeStamp());
    request.setInputPerfMetricReportPolicy(InputToLoadPerfMetricReportPolicy::ReportLink);

    ReferrerPolicy policy;
    if (hasAttribute(referrerpolicyAttr) && SecurityPolicy::referrerPolicyFromString(fastGetAttribute(referrerpolicyAttr), &policy) && !hasRel(RelationNoReferrer)) {
        request.setHTTPReferrer(SecurityPolicy::generateReferrer(policy, completedURL, document().outgoingReferrer()));
    }

    if (hasAttribute(downloadAttr)) {
        request.setRequestContext(WebURLRequest::RequestContextDownload);
        bool isSameOrigin = completedURL.protocolIsData() || document().getSecurityOrigin()->canRequest(completedURL);
        const AtomicString& suggestedName = (isSameOrigin ? fastGetAttribute(downloadAttr) : nullAtom);

        frame->loader().client()->loadURLExternally(request, NavigationPolicyDownload, suggestedName, false);
    } else {
        request.setRequestContext(WebURLRequest::RequestContextHyperlink);
        FrameLoadRequest frameRequest(&document(), request, getAttribute(targetAttr));
        frameRequest.setTriggeringEvent(event);
        if (hasRel(RelationNoReferrer)) {
            frameRequest.setShouldSendReferrer(NeverSendReferrer);
            frameRequest.setShouldSetOpener(NeverSetOpener);
        }
        if (hasRel(RelationNoOpener))
            frameRequest.setShouldSetOpener(NeverSetOpener);
        // TODO(japhet): Link clicks can be emulated via JS without a user gesture.
        // Why doesn't this go through NavigationScheduler?
        frame->loader().load(frameRequest);
    }
}

bool isEnterKeyKeydownEvent(Event* event)
{
    return event->type() == EventTypeNames::keydown && event->isKeyboardEvent() && toKeyboardEvent(event)->key() == "Enter" && !toKeyboardEvent(event)->repeat();
}

bool isLinkClick(Event* event)
{
    // Allow detail <= 1 so that synthetic clicks work. They may have detail == 0.
    return event->type() == EventTypeNames::click && (!event->isMouseEvent() || (toMouseEvent(event)->button() != RightButton && toMouseEvent(event)->detail() <= 1));
}

bool HTMLAnchorElement::willRespondToMouseClickEvents()
{
    return isLink() || HTMLElement::willRespondToMouseClickEvents();
}

bool HTMLAnchorElement::isInteractiveContent() const
{
    return isLink();
}

Node::InsertionNotificationRequest HTMLAnchorElement::insertedInto(ContainerNode* insertionPoint)
{
    InsertionNotificationRequest request = HTMLElement::insertedInto(insertionPoint);
    logAddElementIfIsolatedWorldAndInDocument("a", hrefAttr);
    return request;
}

HTMLAnchorElement::NavigationHintSender* HTMLAnchorElement::ensureNavigationHintSender()
{
    if (!m_navigationHintSender)
        m_navigationHintSender = NavigationHintSender::create(this);
    return m_navigationHintSender;
}

} // namespace blink
