// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PageAnimator.h"

#include "core/animation/DocumentAnimations.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/Logging.h"

namespace blink {

PageAnimator::PageAnimator(Page& page)
    : m_page(page)
    , m_servicingAnimations(false)
    , m_updatingLayoutAndStyleForPainting(false)
{
}

PageAnimator* PageAnimator::create(Page& page)
{
    return new PageAnimator(page);
}

DEFINE_TRACE(PageAnimator)
{
    visitor->trace(m_page);
}

void PageAnimator::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    TemporaryChange<bool> servicing(m_servicingAnimations, true);
    clock().updateTime(monotonicAnimationStartTime);

    HeapVector<Member<Document>, 32> documents;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
            documents.append(toLocalFrame(frame)->document());
    }

    for (auto& document : documents) {
        ScopedFrameBlamer frameBlamer(document->frame());
        TRACE_EVENT0("blink", "PageAnimator::serviceScriptedAnimations");
        DocumentAnimations::updateAnimationTimingForAnimationFrame(*document);
        if (document->view()) {
            if (document->view()->shouldThrottleRendering())
                continue;
            // Disallow throttling in case any script needs to do a synchronous
            // lifecycle update in other frames which are throttled.
            DocumentLifecycle::DisallowThrottlingScope noThrottlingScope(document->lifecycle());
            if (ScrollableArea* scrollableArea = document->view()->getScrollableArea())
                scrollableArea->serviceScrollAnimations(monotonicAnimationStartTime);

            if (const FrameView::ScrollableAreaSet* animatingScrollableAreas = document->view()->animatingScrollableAreas()) {
                // Iterate over a copy, since ScrollableAreas may deregister
                // themselves during the iteration.
                HeapVector<Member<ScrollableArea>> animatingScrollableAreasCopy;
                copyToVector(*animatingScrollableAreas, animatingScrollableAreasCopy);
                for (ScrollableArea* scrollableArea : animatingScrollableAreasCopy)
                    scrollableArea->serviceScrollAnimations(monotonicAnimationStartTime);
            }
            SVGDocumentExtensions::serviceOnAnimationFrame(*document);
        }
        // TODO(skyostil): This function should not run for documents without views.
        DocumentLifecycle::DisallowThrottlingScope noThrottlingScope(document->lifecycle());
        document->serviceScriptedAnimations(monotonicAnimationStartTime);
    }
}

void PageAnimator::scheduleVisualUpdate(LocalFrame* frame)
{
    if (m_servicingAnimations || m_updatingLayoutAndStyleForPainting)
        return;
    m_page->chromeClient().scheduleAnimation(frame->view());
}

void PageAnimator::updateAllLifecyclePhases(LocalFrame& rootFrame)
{
    FrameView* view = rootFrame.view();
    TemporaryChange<bool> servicing(m_updatingLayoutAndStyleForPainting, true);
    view->updateAllLifecyclePhases();
}

} // namespace blink
