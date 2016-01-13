// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PageAnimator.h"

#include "core/animation/DocumentAnimations.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/svg/SVGDocumentExtensions.h"

namespace WebCore {

PageAnimator::PageAnimator(Page* page)
    : m_page(page)
    , m_animationFramePending(false)
    , m_servicingAnimations(false)
    , m_updatingLayoutAndStyleForPainting(false)
{
}

void PageAnimator::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    m_animationFramePending = false;
    TemporaryChange<bool> servicing(m_servicingAnimations, true);

    for (RefPtr<Frame> frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame()) {
            RefPtr<LocalFrame> localFrame = toLocalFrame(frame.get());
            localFrame->view()->serviceScrollAnimations();

            DocumentAnimations::updateAnimationTimingForAnimationFrame(*localFrame->document(), monotonicAnimationStartTime);
            SVGDocumentExtensions::serviceOnAnimationFrame(*localFrame->document(), monotonicAnimationStartTime);
        }
    }

    WillBeHeapVector<RefPtrWillBeMember<Document> > documents;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
            documents.append(toLocalFrame(frame)->document());
    }

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(monotonicAnimationStartTime);
}

void PageAnimator::scheduleVisualUpdate()
{
    // FIXME: also include m_animationFramePending here. It is currently not there due to crbug.com/353756.
    if (m_servicingAnimations || m_updatingLayoutAndStyleForPainting)
        return;
    m_page->chrome().scheduleAnimation();
}

void PageAnimator::updateLayoutAndStyleForPainting()
{
    if (!m_page->mainFrame()->isLocalFrame())
        return;

    RefPtr<FrameView> view = m_page->deprecatedLocalMainFrame()->view();

    TemporaryChange<bool> servicing(m_updatingLayoutAndStyleForPainting, true);

    // In order for our child HWNDs (NativeWindowWidgets) to update properly,
    // they need to be told that we are updating the screen. The problem is that
    // the native widgets need to recalculate their clip region and not overlap
    // any of our non-native widgets. To force the resizing, call
    // setFrameRect(). This will be a quick operation for most frames, but the
    // NativeWindowWidgets will update a proper clipping region.
    view->setFrameRect(view->frameRect());

    // setFrameRect may have the side-effect of causing existing page layout to
    // be invalidated, so layout needs to be called last.
    view->updateLayoutAndStyleForPainting();
}

}
