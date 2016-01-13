/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "core/rendering/RenderPart.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/RenderSVGRoot.h"

using namespace std;

namespace WebCore {

RenderPart::RenderPart(Element* node)
    : RenderWidget(node)
{
    setInline(false);
}

RenderPart::~RenderPart()
{
}

LayerType RenderPart::layerTypeRequired() const
{
    LayerType type = RenderWidget::layerTypeRequired();
    if (type != NoLayer)
        return type;
    return ForcedLayer;
}

bool RenderPart::requiresAcceleratedCompositing() const
{
    // There are two general cases in which we can return true. First, if this is a plugin
    // renderer and the plugin has a layer, then we need a layer. Second, if this is
    // a renderer with a contentDocument and that document needs a layer, then we need
    // a layer.
    if (widget() && widget()->isPluginView() && toPluginView(widget())->platformLayer())
        return true;

    if (!node() || !node()->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* element = toHTMLFrameOwnerElement(node());
    if (element->contentFrame() && element->contentFrame()->remotePlatformLayer())
        return true;

    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->usesCompositing();
    }

    return false;
}

bool RenderPart::needsPreferredWidthsRecalculation() const
{
    if (RenderWidget::needsPreferredWidthsRecalculation())
        return true;
    return embeddedContentBox();
}

bool RenderPart::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    if (!widget() || !widget()->isFrameView() || !request.allowsChildFrameContent())
        return RenderWidget::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);

    // FIXME: Until RemoteFrames use RemoteFrameViews, we need an explicit check here.
    if (toFrameView(widget())->frame().isRemoteFrameTemporary())
        return RenderWidget::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);

    FrameView* childFrameView = toFrameView(widget());
    RenderView* childRoot = childFrameView->renderView();

    if (childRoot) {
        LayoutPoint adjustedLocation = accumulatedOffset + location();
        LayoutPoint contentOffset = LayoutPoint(borderLeft() + paddingLeft(), borderTop() + paddingTop()) - childFrameView->scrollOffset();
        HitTestLocation newHitTestLocation(locationInContainer, -adjustedLocation - contentOffset);
        HitTestRequest newHitTestRequest(request.type() | HitTestRequest::ChildFrameHitTest);
        HitTestResult childFrameResult(newHitTestLocation);

        bool isInsideChildFrame = childRoot->hitTest(newHitTestRequest, newHitTestLocation, childFrameResult);

        if (newHitTestLocation.isRectBasedTest())
            result.append(childFrameResult);
        else if (isInsideChildFrame)
            result = childFrameResult;

        if (isInsideChildFrame)
            return true;

        if (request.allowsFrameScrollbars()) {
            // ScrollView scrollbars are not the same as RenderLayer scrollbars tested by RenderLayer::hitTestOverflowControls,
            // so we need to test ScrollView scrollbars separately here.
            // FIXME: Consider if this test could be done unconditionally.
            Scrollbar* frameScrollbar = childFrameView->scrollbarAtPoint(newHitTestLocation.roundedPoint());
            if (frameScrollbar)
                result.setScrollbar(frameScrollbar);
        }
    }

    return RenderWidget::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);
}

CompositingReasons RenderPart::additionalCompositingReasons(CompositingTriggerFlags) const
{
    if (requiresAcceleratedCompositing())
        return CompositingReasonIFrame;
    return CompositingReasonNone;
}

}
