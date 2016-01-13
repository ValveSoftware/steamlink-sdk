/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/frame/SmartClip.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/Page.h"
#include "core/rendering/RenderObject.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static IntRect applyScaleWithoutCollapsingToZero(const IntRect& rect, float scale)
{
    IntRect result = rect;
    result.scale(scale);
    if (rect.width() > 0 && !result.width())
        result.setWidth(1);
    if (rect.height() > 0 && !result.height())
        result.setHeight(1);
    return result;
}

static Node* nodeInsideFrame(Node* node)
{
    if (node->isFrameOwnerElement())
        return toHTMLFrameOwnerElement(node)->contentDocument();
    return 0;
}

IntRect SmartClipData::rect() const
{
    return m_rect;
}

const String& SmartClipData::clipData() const
{
    return m_string;
}

SmartClip::SmartClip(PassRefPtr<LocalFrame> frame)
    : m_frame(frame)
{
}

SmartClipData SmartClip::dataForRect(const IntRect& cropRect)
{
    IntRect resizedCropRect = applyScaleWithoutCollapsingToZero(cropRect, 1 / pageScaleFactor());

    Node* bestNode = findBestOverlappingNode(m_frame->document(), resizedCropRect);
    if (!bestNode)
        return SmartClipData();

    if (Node* nodeFromFrame = nodeInsideFrame(bestNode)) {
        // FIXME: This code only hit-tests a single iframe. It seems like we ought support nested frames.
        if (Node* bestNodeInFrame = findBestOverlappingNode(nodeFromFrame, resizedCropRect))
            bestNode = bestNodeInFrame;
    }

    WillBeHeapVector<RawPtrWillBeMember<Node> > hitNodes;
    collectOverlappingChildNodes(bestNode, resizedCropRect, hitNodes);

    if (hitNodes.isEmpty() || hitNodes.size() == bestNode->countChildren()) {
        hitNodes.clear();
        hitNodes.append(bestNode);
    }

    // Unite won't work with the empty rect, so we initialize to the first rect.
    IntRect unitedRects = hitNodes[0]->pixelSnappedBoundingBox();
    StringBuilder collectedText;
    for (size_t i = 0; i < hitNodes.size(); ++i) {
        collectedText.append(extractTextFromNode(hitNodes[i]));
        unitedRects.unite(hitNodes[i]->pixelSnappedBoundingBox());
    }

    return SmartClipData(bestNode, convertRectToWindow(unitedRects), collectedText.toString());
}

float SmartClip::pageScaleFactor()
{
    return m_frame->page()->pageScaleFactor();
}

// This function is a bit of a mystery. If you understand what it does, please
// consider adding a more descriptive name.
Node* SmartClip::minNodeContainsNodes(Node* minNode, Node* newNode)
{
    if (!newNode)
        return minNode;
    if (!minNode)
        return newNode;

    IntRect minNodeRect = minNode->pixelSnappedBoundingBox();
    IntRect newNodeRect = newNode->pixelSnappedBoundingBox();

    Node* parentMinNode = minNode->parentNode();
    Node* parentNewNode = newNode->parentNode();

    if (minNodeRect.contains(newNodeRect)) {
        if (parentMinNode && parentNewNode && parentNewNode->parentNode() == parentMinNode)
            return parentMinNode;
        return minNode;
    }

    if (newNodeRect.contains(minNodeRect)) {
        if (parentMinNode && parentNewNode && parentMinNode->parentNode() == parentNewNode)
            return parentNewNode;
        return newNode;
    }

    // This loop appears to find the nearest ancestor of minNode (in DOM order)
    // that contains the newNodeRect. It's very unclear to me why that's an
    // interesting node to find. Presumably this loop will often just return
    // the documentElement.
    Node* node = minNode;
    while (node) {
        if (node->renderer()) {
            IntRect nodeRect = node->pixelSnappedBoundingBox();
            if (nodeRect.contains(newNodeRect)) {
                return node;
            }
        }
        node = node->parentNode();
    }

    return 0;
}

Node* SmartClip::findBestOverlappingNode(Node* rootNode, const IntRect& cropRect)
{
    if (!rootNode)
        return 0;

    IntRect resizedCropRect = rootNode->document().view()->windowToContents(cropRect);

    Node* node = rootNode;
    Node* minNode = 0;

    while (node) {
        IntRect nodeRect = node->pixelSnappedBoundingBox();

        if (node->isElementNode() && equalIgnoringCase(toElement(node)->fastGetAttribute(HTMLNames::aria_hiddenAttr), "true")) {
            node = NodeTraversal::nextSkippingChildren(*node, rootNode);
            continue;
        }

        RenderObject* renderer = node->renderer();
        if (renderer && !nodeRect.isEmpty()) {
            if (renderer->isText()
                || renderer->isRenderImage()
                || node->isFrameOwnerElement()
                || (renderer->style()->hasBackgroundImage() && !shouldSkipBackgroundImage(node))) {
                if (resizedCropRect.intersects(nodeRect)) {
                    minNode = minNodeContainsNodes(minNode, node);
                } else {
                    node = NodeTraversal::nextSkippingChildren(*node, rootNode);
                    continue;
                }
            }
        }
        node = NodeTraversal::next(*node, rootNode);
    }

    return minNode;
}

// This function appears to heuristically guess whether to include a background
// image in the smart clip. It seems to want to include sprites created from
// CSS background images but to skip actual backgrounds.
bool SmartClip::shouldSkipBackgroundImage(Node* node)
{
    ASSERT(node);
    // Apparently we're only interested in background images on spans and divs.
    if (!isHTMLSpanElement(*node) && !isHTMLDivElement(*node))
        return true;

    // This check actually makes a bit of sense. If you're going to sprite an
    // image out of a CSS background, you're probably going to specify a height
    // or a width. On the other hand, if we've got a legit background image,
    // it's very likely the height or the width will be set to auto.
    RenderObject* renderer = node->renderer();
    if (renderer && (renderer->style()->logicalHeight().isAuto() || renderer->style()->logicalWidth().isAuto()))
        return true;

    return false;
}

void SmartClip::collectOverlappingChildNodes(Node* parentNode, const IntRect& cropRect, WillBeHeapVector<RawPtrWillBeMember<Node> >& hitNodes)
{
    if (!parentNode)
        return;
    IntRect resizedCropRect = parentNode->document().view()->windowToContents(cropRect);
    for (Node* child = parentNode->firstChild(); child; child = child->nextSibling()) {
        IntRect childRect = child->pixelSnappedBoundingBox();
        if (resizedCropRect.intersects(childRect))
            hitNodes.append(child);
    }
}

IntRect SmartClip::convertRectToWindow(const IntRect& nodeRect)
{
    IntRect result = m_frame->document()->view()->contentsToWindow(nodeRect);
    result.scale(pageScaleFactor());
    return result;
}

String SmartClip::extractTextFromNode(Node* node)
{
    // Science has proven that no text nodes are ever positioned at y == -99999.
    int prevYPos = -99999;

    StringBuilder result;
    for (Node* currentNode = node; currentNode; currentNode = NodeTraversal::next(*currentNode, node)) {
        RenderStyle* style = currentNode->computedStyle();
        if (style && style->userSelect() == SELECT_NONE)
            continue;

        if (Node* nodeFromFrame = nodeInsideFrame(currentNode))
            result.append(extractTextFromNode(nodeFromFrame));

        IntRect nodeRect = currentNode->pixelSnappedBoundingBox();
        if (currentNode->renderer() && !nodeRect.isEmpty()) {
            if (currentNode->isTextNode()) {
                String nodeValue = currentNode->nodeValue();

                // It's unclear why we blacklist solitary "\n" node values.
                // Maybe we're trying to ignore <br> tags somehow?
                if (nodeValue == "\n")
                    nodeValue = "";

                if (nodeRect.y() != prevYPos) {
                    prevYPos = nodeRect.y();
                    result.append('\n');
                }

                result.append(nodeValue);
            }
        }
    }

    return result.toString();
}

} // namespace WebCore
