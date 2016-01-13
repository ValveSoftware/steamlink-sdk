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

#include "config.h"
#include "core/html/HTMLFrameOwnerElement.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace WebCore {

typedef HashMap<RefPtr<Widget>, FrameView*> WidgetToParentMap;
static WidgetToParentMap& widgetNewParentMap()
{
    DEFINE_STATIC_LOCAL(WidgetToParentMap, map, ());
    return map;
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
    WidgetToParentMap::iterator end = map.end();
    for (WidgetToParentMap::iterator it = map.begin(); it != end; ++it) {
        Widget* child = it->key.get();
        ScrollView* currentParent = toScrollView(child->parent());
        FrameView* newParent = it->value;
        if (newParent != currentParent) {
            if (currentParent)
                currentParent->removeChild(child);
            if (newParent)
                newParent->addChild(child);
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

static void moveWidgetToParentSoon(Widget* child, FrameView* parent)
{
    if (!s_updateSuspendCount) {
        if (parent)
            parent->addChild(child);
        else if (toScrollView(child->parent()))
            toScrollView(child->parent())->removeChild(child);
        return;
    }
    widgetNewParentMap().set(child, parent);
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_contentFrame(0)
    , m_widget(nullptr)
    , m_sandboxFlags(SandboxNone)
{
}

RenderPart* HTMLFrameOwnerElement::renderPart() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isRenderPart())
        return 0;
    return toRenderPart(renderer());
}

void HTMLFrameOwnerElement::setContentFrame(Frame& frame)
{
    // Make sure we will not end up with two frames referencing the same owner element.
    ASSERT(!m_contentFrame || m_contentFrame->owner() != this);
    // Disconnected frames should not be allowed to load.
    ASSERT(inDocument());
    m_contentFrame = &frame;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame()
{
    if (!m_contentFrame)
        return;

    m_contentFrame = 0;

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
        RefPtr<Frame> protect(frame);
        if (frame->isLocalFrame())
            toLocalFrame(frame)->loader().frameDetached();
        frame->disconnectOwnerElement();
    }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement()
{
    if (m_contentFrame)
        m_contentFrame->disconnectOwnerElement();
}

Document* HTMLFrameOwnerElement::contentDocument() const
{
    return (m_contentFrame && m_contentFrame->isLocalFrame()) ? toLocalFrame(m_contentFrame)->document() : 0;
}

LocalDOMWindow* HTMLFrameOwnerElement::contentWindow() const
{
    return m_contentFrame ? m_contentFrame->domWindow() : 0;
}

void HTMLFrameOwnerElement::setSandboxFlags(SandboxFlags flags)
{
    m_sandboxFlags = flags;
}

bool HTMLFrameOwnerElement::isKeyboardFocusable() const
{
    return m_contentFrame && HTMLElement::isKeyboardFocusable();
}

void HTMLFrameOwnerElement::dispatchLoad()
{
    dispatchEvent(Event::create(EventTypeNames::load));
}

Document* HTMLFrameOwnerElement::getSVGDocument(ExceptionState& exceptionState) const
{
    Document* doc = contentDocument();
    if (doc && doc->isSVGDocument())
        return doc;
    return 0;
}

void HTMLFrameOwnerElement::setWidget(PassRefPtr<Widget> widget)
{
    if (widget == m_widget)
        return;

    if (m_widget) {
        if (m_widget->parent())
            moveWidgetToParentSoon(m_widget.get(), 0);
        m_widget = nullptr;
    }

    m_widget = widget;

    RenderWidget* renderWidget = toRenderWidget(renderer());
    if (!renderWidget)
        return;

    if (m_widget) {
        renderWidget->updateOnWidgetChange();

        ASSERT(document().view() == renderWidget->frameView());
        ASSERT(renderWidget->frameView());
        moveWidgetToParentSoon(m_widget.get(), renderWidget->frameView());
    }

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(renderWidget);
}

Widget* HTMLFrameOwnerElement::ownedWidget() const
{
    return m_widget.get();
}

bool HTMLFrameOwnerElement::loadOrRedirectSubframe(const KURL& url, const AtomicString& frameName, bool lockBackForwardList)
{
    RefPtr<LocalFrame> parentFrame = document().frame();
    // FIXME(kenrb): The necessary semantics for RemoteFrames have not been worked out yet, but this will likely need some logic to handle them.
    if (contentFrame() && contentFrame()->isLocalFrame()) {
        toLocalFrame(contentFrame())->navigationScheduler().scheduleLocationChange(&document(), url.string(), Referrer(document().outgoingReferrer(), document().referrerPolicy()), lockBackForwardList);
        return true;
    }

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(parentFrame.get(), url.string());
        return false;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return false;

    String referrer = SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), url, document().outgoingReferrer());
    RefPtr<LocalFrame> childFrame = parentFrame->loader().client()->createFrame(url, frameName, Referrer(referrer, document().referrerPolicy()), this);

    if (!childFrame)  {
        parentFrame->loader().checkCompleted();
        return false;
    }

    // All new frames will have m_isComplete set to true at this point due to synchronously loading
    // an empty document in FrameLoader::init(). But many frames will now be starting an
    // asynchronous load of url, so we set m_isComplete to false and then check if the load is
    // actually completed below. (Note that we set m_isComplete to false even for synchronous
    // loads, so that checkCompleted() below won't bail early.)
    // FIXME: Can we remove this entirely? m_isComplete normally gets set to false when a load is committed.
    childFrame->loader().started();

    FrameView* view = childFrame->view();
    RenderObject* renderObject = renderer();
    // We need to test the existence of renderObject and its widget-ness, as
    // failing to do so causes problems.
    if (renderObject && renderObject->isWidget() && view)
        setWidget(view);

    // Some loads are performed synchronously (e.g., about:blank and loads
    // cancelled by returning a null ResourceRequest from requestFromDelegate).
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the
    // completed() signal for the child by hand and mark the load as being
    // complete.
    // FIXME: In this case the LocalFrame will have finished loading before
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (childFrame->loader().state() == FrameStateComplete && !childFrame->loader().policyDocumentLoader())
        childFrame->loader().checkCompleted();
    return true;
}


} // namespace WebCore
