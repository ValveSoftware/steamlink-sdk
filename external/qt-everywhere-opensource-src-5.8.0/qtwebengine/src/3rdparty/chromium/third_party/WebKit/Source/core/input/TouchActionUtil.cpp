// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchActionUtil.h"

#include "core/dom/Node.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace TouchActionUtil {

namespace {

// touch-action applies to all elements with both width AND height properties.
// According to the CSS Box Model Spec (http://dev.w3.org/csswg/css-box/#the-width-and-height-properties)
// width applies to all elements but non-replaced inline elements, table rows, and row groups and
// height applies to all elements but non-replaced inline elements, table columns, and column groups.
bool supportsTouchAction(const LayoutObject& object)
{
    if (object.isInline() && !object.isAtomicInlineLevel())
        return false;
    if (object.isTableRow() || object.isLayoutTableCol())
        return false;

    return true;
}

const Node* parentNodeAcrossFrames(const Node* curNode)
{
    Node* parentNode = FlatTreeTraversal::parent(*curNode);
    if (parentNode)
        return parentNode;

    if (curNode->isDocumentNode()) {
        const Document* doc = toDocument(curNode);
        return doc->localOwner();
    }

    return nullptr;
}

} // namespace

TouchAction computeEffectiveTouchAction(const Node& node)
{
    // Start by permitting all actions, then walk the elements supporting
    // touch-action from the target node up to root document, exclude any
    // prohibited actions at or below the element that supports them.
    // I.e. pan-related actions are considered up to the nearest scroller,
    // and zoom related actions are considered up to the root.
    TouchAction effectiveTouchAction = TouchActionAuto;
    TouchAction handledTouchActions = TouchActionNone;
    for (const Node* curNode = &node; curNode; curNode = parentNodeAcrossFrames(curNode)) {
        if (LayoutObject* layoutObject = curNode->layoutObject()) {
            if (supportsTouchAction(*layoutObject)) {
                TouchAction action = layoutObject->style()->getTouchAction();
                action |= handledTouchActions;
                effectiveTouchAction &= action;
                if (effectiveTouchAction == TouchActionNone)
                    break;
            }

            // If we've reached an ancestor that supports panning, stop allowing panning to be disabled.
            if ((layoutObject->isBox() && toLayoutBox(layoutObject)->scrollsOverflow())
                || layoutObject->isLayoutView())
                handledTouchActions |= TouchActionPan;
        }
    }
    return effectiveTouchAction;
}

} // namespace TouchActionUtil
} // namespace blink
