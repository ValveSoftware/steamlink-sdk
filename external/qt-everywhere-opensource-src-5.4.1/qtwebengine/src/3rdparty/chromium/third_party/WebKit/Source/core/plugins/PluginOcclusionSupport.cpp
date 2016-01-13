/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/plugins/PluginOcclusionSupport.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderObject.h"
#include "platform/Widget.h"
#include "wtf/HashSet.h"

// This file provides a utility function to support rendering certain elements above plugins.

namespace WebCore {

static void getObjectStack(const RenderObject* ro, Vector<const RenderObject*>* roStack)
{
    roStack->clear();
    while (ro) {
        roStack->append(ro);
        ro = ro->parent();
    }
}

// Returns true if stack1 is at or above stack2
static bool iframeIsAbovePlugin(const Vector<const RenderObject*>& iframeZstack, const Vector<const RenderObject*>& pluginZstack)
{
    for (size_t i = 0; i < iframeZstack.size() && i < pluginZstack.size(); i++) {
        // The root is at the end of these stacks. We want to iterate
        // root-downwards so we index backwards from the end.
        const RenderObject* ro1 = iframeZstack[iframeZstack.size() - 1 - i];
        const RenderObject* ro2 = pluginZstack[pluginZstack.size() - 1 - i];

        if (ro1 != ro2) {
            // When we find nodes in the stack that are not the same, then
            // we've found the nodes just below the lowest comment ancestor.
            // Determine which should be on top.

            // See if z-index determines an order.
            if (ro1->style() && ro2->style()) {
                int z1 = ro1->style()->zIndex();
                int z2 = ro2->style()->zIndex();
                if (z1 > z2)
                    return true;
                if (z1 < z2)
                    return false;
            }

            // If the plugin does not have an explicit z-index it stacks behind the iframe.
            // This is for maintaining compatibility with IE.
            if (ro2->style()->position() == StaticPosition) {
                // The 0'th elements of these RenderObject arrays represent the plugin node and
                // the iframe.
                const RenderObject* pluginRenderObject = pluginZstack[0];
                const RenderObject* iframeRenderObject = iframeZstack[0];

                if (pluginRenderObject->style() && iframeRenderObject->style()) {
                    if (pluginRenderObject->style()->zIndex() > iframeRenderObject->style()->zIndex())
                        return false;
                }
                return true;
            }

            // Inspect the document order. Later order means higher stacking.
            const RenderObject* parent = ro1->parent();
            if (!parent)
                return false;
            ASSERT(parent == ro2->parent());

            for (const RenderObject* ro = parent->slowFirstChild(); ro; ro = ro->nextSibling()) {
                if (ro == ro1)
                    return false;
                if (ro == ro2)
                    return true;
            }
            ASSERT(false); // We should have seen ro1 and ro2 by now.
            return false;
        }
    }
    return true;
}

static bool intersectsRect(const RenderObject* renderer, const IntRect& rect)
{
    return renderer->absoluteBoundingBoxRectIgnoringTransforms().intersects(rect)
        && (!renderer->style() || renderer->style()->visibility() == VISIBLE);
}

static void addToOcclusions(const RenderBox* renderer, Vector<IntRect>& occlusions)
{
    IntPoint point = roundedIntPoint(renderer->localToAbsolute());
    IntSize size(renderer->width(), renderer->height());
    occlusions.append(IntRect(point, size));
}

static void addTreeToOcclusions(const RenderObject* renderer, const IntRect& frameRect, Vector<IntRect>& occlusions)
{
    if (!renderer)
        return;
    if (renderer->isBox() && intersectsRect(renderer, frameRect))
        addToOcclusions(toRenderBox(renderer), occlusions);
    for (RenderObject* child = renderer->slowFirstChild(); child; child = child->nextSibling())
        addTreeToOcclusions(child, frameRect, occlusions);
}

static const Element* topLayerAncestor(const Element* element)
{
    while (element && !element->isInTopLayer())
        element = element->parentOrShadowHostElement();
    return element;
}

// Return a set of rectangles that should not be overdrawn by the
// plugin ("cutouts"). This helps implement the "iframe shim"
// technique of overlaying a windowed plugin with content from the
// page. In a nutshell, iframe elements should occlude plugins when
// they occur higher in the stacking order.
void getPluginOcclusions(Element* element, Widget* parentWidget, const IntRect& frameRect, Vector<IntRect>& occlusions)
{
    RenderObject* pluginNode = element->renderer();
    ASSERT(pluginNode);
    if (!pluginNode->style())
        return;
    Vector<const RenderObject*> pluginZstack;
    Vector<const RenderObject*> iframeZstack;
    getObjectStack(pluginNode, &pluginZstack);

    if (!parentWidget->isFrameView())
        return;

    FrameView* parentFrameView = toFrameView(parentWidget);

    // Occlusions by iframes.
    const HashSet<RefPtr<Widget> >* children = parentFrameView->children();
    for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(); it != children->end(); ++it) {
        // We only care about FrameView's because iframes show up as FrameViews.
        if (!(*it)->isFrameView())
            continue;

        const FrameView* frameView = toFrameView((*it).get());
        // Check to make sure we can get both the element and the RenderObject
        // for this FrameView, if we can't just move on to the next object.
        // FIXME: Plugin occlusion by remote frames is probably broken.
        HTMLElement* element = frameView->frame().deprecatedLocalOwner();
        if (!element || !element->renderer())
            continue;

        RenderObject* iframeRenderer = element->renderer();

        if (isHTMLIFrameElement(*element) && intersectsRect(iframeRenderer, frameRect)) {
            getObjectStack(iframeRenderer, &iframeZstack);
            if (iframeIsAbovePlugin(iframeZstack, pluginZstack))
                addToOcclusions(toRenderBox(iframeRenderer), occlusions);
        }
    }

    // Occlusions by top layer elements.
    // FIXME: There's no handling yet for the interaction between top layer and
    // iframes. For example, a plugin in the top layer will be occluded by an
    // iframe. And a plugin inside an iframe in the top layer won't be respected
    // as being in the top layer.
    const Element* ancestor = topLayerAncestor(element);
    Document* document = parentFrameView->frame().document();
    const WillBeHeapVector<RefPtrWillBeMember<Element> >& elements = document->topLayerElements();
    size_t start = ancestor ? elements.find(ancestor) + 1 : 0;
    for (size_t i = start; i < elements.size(); ++i)
        addTreeToOcclusions(elements[i]->renderer(), frameRect, occlusions);
}

} // namespace WebCore
