/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#include "core/html/HTMLFrameOwnerElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/plugins/PluginView.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

typedef HeapHashMap<Member<Widget>, Member<FrameView>> WidgetToParentMap;
static WidgetToParentMap& widgetNewParentMap()
{
    DEFINE_STATIC_LOCAL(WidgetToParentMap, map, (new WidgetToParentMap));
    return map;
}

using WidgetSet = HeapHashSet<Member<Widget>>;
static WidgetSet& widgetsPendingTemporaryRemovalFromParent()
{
    // Widgets in this set will not leak because it will be cleared in
    // HTMLFrameOwnerElement::UpdateSuspendScope::performDeferredWidgetTreeOperations.
    DEFINE_STATIC_LOCAL(WidgetSet, set, (new WidgetSet));
    return set;
}

static WidgetSet& widgetsPendingDispose()
{
    DEFINE_STATIC_LOCAL(WidgetSet, set, (new WidgetSet));
    return set;
}

SubframeLoadingDisabler::SubtreeRootSet& SubframeLoadingDisabler::disabledSubtreeRoots()
{
    DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, (new SubtreeRootSet));
    return nodes;
}

static unsigned s_updateSuspendCount = 0;

HTMLFrameOwnerElement::UpdateSuspendScope::UpdateSuspendScope()
{
    ++s_updateSuspendCount;
}

void HTMLFrameOwnerElement::UpdateSuspendScope::performDeferredWidgetTreeOperations()
{
    WidgetToParentMap map;
    widgetNewParentMap().swap(map);
    for (const auto& widget : map) {
        Widget* child = widget.key.get();
        FrameView* currentParent = toFrameView(child->parent());
        FrameView* newParent = widget.value;
        if (newParent != currentParent) {
            if (currentParent)
                currentParent->removeChild(child);
            if (newParent)
                newParent->addChild(child);
            if (currentParent && !newParent)
                child->dispose();
        }
    }

    {
        WidgetSet set;
        widgetsPendingTemporaryRemovalFromParent().swap(set);
        for (const auto& widget : set) {
            FrameView* currentParent = toFrameView(widget->parent());
            if (currentParent)
                currentParent->removeChild(widget.get());
        }
    }

    {
        WidgetSet set;
        widgetsPendingDispose().swap(set);
        for (const auto& widget : set) {
            widget->dispose();
        }
    }
}

HTMLFrameOwnerElement::UpdateSuspendScope::~UpdateSuspendScope()
{
    ASSERT(s_updateSuspendCount > 0);
    if (s_updateSuspendCount == 1)
        performDeferredWidgetTreeOperations();
    --s_updateSuspendCount;
}

// Unlike moveWidgetToParentSoon, this will not call dispose the Widget.
void temporarilyRemoveWidgetFromParentSoon(Widget* widget)
{
    if (s_updateSuspendCount) {
        widgetsPendingTemporaryRemovalFromParent().add(widget);
    } else {
        if (toFrameView(widget->parent()))
            toFrameView(widget->parent())->removeChild(widget);
    }
}

void moveWidgetToParentSoon(Widget* child, FrameView* parent)
{
    if (!s_updateSuspendCount) {
        if (parent) {
            parent->addChild(child);
        } else if (toFrameView(child->parent())) {
            toFrameView(child->parent())->removeChild(child);
            child->dispose();
        }
        return;
    }
    widgetNewParentMap().set(child, parent);
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_contentFrame(nullptr)
    , m_widget(nullptr)
    , m_sandboxFlags(SandboxNone)
{
}

LayoutPart* HTMLFrameOwnerElement::layoutPart() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
    // when using fallback content.
    if (!layoutObject() || !layoutObject()->isLayoutPart())
        return nullptr;
    return toLayoutPart(layoutObject());
}

void HTMLFrameOwnerElement::setContentFrame(Frame& frame)
{
    // Make sure we will not end up with two frames referencing the same owner element.
    ASSERT(!m_contentFrame || m_contentFrame->owner() != this);
    // Disconnected frames should not be allowed to load.
    ASSERT(inShadowIncludingDocument());
    m_contentFrame = &frame;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame()
{
    if (!m_contentFrame)
        return;

    ASSERT(m_contentFrame->owner() == this);
    m_contentFrame = nullptr;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::disconnectContentFrame()
{
    // FIXME: Currently we don't do this in removedFrom because this causes an
    // unload event in the subframe which could execute script that could then
    // reach up into this document and then attempt to look back down. We should
    // see if this behavior is really needed as Gecko does not allow this.
    if (Frame* frame = contentFrame()) {
        frame->detach(FrameDetachType::Remove);
    }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement()
{
    // An owner must by now have been informed of detachment
    // when the frame was closed.
    ASSERT(!m_contentFrame);
}

Document* HTMLFrameOwnerElement::contentDocument() const
{
    return (m_contentFrame && m_contentFrame->isLocalFrame()) ? toLocalFrame(m_contentFrame)->document() : 0;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const
{
    return m_contentFrame ? m_contentFrame->domWindow() : 0;
}

void HTMLFrameOwnerElement::setSandboxFlags(SandboxFlags flags)
{
    m_sandboxFlags = flags;
    // Don't notify about updates if contentFrame() is null, for example when
    // the subframe hasn't been created yet.
    if (contentFrame())
        document().frame()->loader().client()->didChangeSandboxFlags(contentFrame(), flags);
}

bool HTMLFrameOwnerElement::isKeyboardFocusable() const
{
    return m_contentFrame && HTMLElement::isKeyboardFocusable();
}

void HTMLFrameOwnerElement::disposeWidgetSoon(Widget* widget)
{
    if (s_updateSuspendCount) {
        widgetsPendingDispose().add(widget);
        return;
    }
    widget->dispose();
}

void HTMLFrameOwnerElement::dispatchLoad()
{
    dispatchScopedEvent(Event::create(EventTypeNames::load));
}

const WebVector<WebPermissionType>& HTMLFrameOwnerElement::delegatedPermissions() const
{
    DEFINE_STATIC_LOCAL(WebVector<WebPermissionType>, permissions, ());
    return permissions;
}

Document* HTMLFrameOwnerElement::getSVGDocument(ExceptionState& exceptionState) const
{
    Document* doc = contentDocument();
    if (doc && doc->isSVGDocument())
        return doc;
    return nullptr;
}

void HTMLFrameOwnerElement::setWidget(Widget* widget)
{
    if (widget == m_widget)
        return;

    if (m_widget) {
        if (m_widget->parent())
            moveWidgetToParentSoon(m_widget.get(), 0);
        m_widget = nullptr;
    }

    m_widget = widget;

    LayoutPart* layoutPart = toLayoutPart(layoutObject());
    LayoutPartItem layoutPartItem = LayoutPartItem(layoutPart);
    if (layoutPartItem.isNull())
        return;

    if (m_widget) {
        layoutPartItem.updateOnWidgetChange();

        ASSERT(document().view() == layoutPartItem.frameView());
        ASSERT(layoutPartItem.frameView());
        moveWidgetToParentSoon(m_widget.get(), layoutPartItem.frameView());
    }

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(layoutPart);
}

Widget* HTMLFrameOwnerElement::releaseWidget()
{
    if (!m_widget)
        return nullptr;
    if (m_widget->parent())
        temporarilyRemoveWidgetFromParentSoon(m_widget.get());
    LayoutPart* layoutPart = toLayoutPart(layoutObject());
    if (layoutPart) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            cache->childrenChanged(layoutPart);
    }
    return m_widget.release();
}

Widget* HTMLFrameOwnerElement::ownedWidget() const
{
    return m_widget.get();
}

bool HTMLFrameOwnerElement::loadOrRedirectSubframe(const KURL& url, const AtomicString& frameName, bool replaceCurrentItem)
{
    LocalFrame* parentFrame = document().frame();
    if (contentFrame()) {
        contentFrame()->navigate(document(), url, replaceCurrentItem, UserGestureStatus::None);
        return true;
    }

    if (!document().getSecurityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(parentFrame, url.getString());
        return false;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return false;

    if (document().frame()->host()->subframeCount() >= FrameHost::maxNumberOfFrames)
        return false;

    FrameLoadRequest frameLoadRequest(&document(), url, "_self", CheckContentSecurityPolicy);

    ReferrerPolicy policy = referrerPolicyAttribute();
    if (policy != ReferrerPolicyDefault)
        frameLoadRequest.resourceRequest().setHTTPReferrer(SecurityPolicy::generateReferrer(policy, url, document().outgoingReferrer()));

    return parentFrame->loader().client()->createFrame(frameLoadRequest, frameName, this);
}

DEFINE_TRACE(HTMLFrameOwnerElement)
{
    visitor->trace(m_contentFrame);
    visitor->trace(m_widget);
    HTMLElement::trace(visitor);
    FrameOwner::trace(visitor);
}


} // namespace blink
