// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(DragUpdateTest, AffectedByDragUpdate)
{
    // Check that when dragging the div in the document below, you only get a
    // single element style recalc.

    OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    HTMLDocument* document = toHTMLDocument(&dummyPageHolder->document());
    document->documentElement()->setInnerHTML("<style>div {width:100px;height:100px} div:-webkit-drag { background-color: green }</style>"
        "<div>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "</div>", ASSERT_NO_EXCEPTION);

    document->view()->updateLayoutAndStyleIfNeededRecursive();
    unsigned startCount = document->styleEngine()->resolverAccessCount();

    document->documentElement()->renderer()->updateDragState(true);
    document->view()->updateLayoutAndStyleIfNeededRecursive();

    unsigned accessCount = document->styleEngine()->resolverAccessCount() - startCount;

    ASSERT_EQ(1U, accessCount);
}

TEST(DragUpdateTest, ChildrenOrSiblingsAffectedByDragUpdate)
{
    // Check that when dragging the div in the document below, you get a
    // full subtree style recalc.

    OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    HTMLDocument* document = toHTMLDocument(&dummyPageHolder->document());
    document->documentElement()->setInnerHTML("<style>div {width:100px;height:100px} div:-webkit-drag span { background-color: green }</style>"
        "<div>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "<span></span>"
        "</div>", ASSERT_NO_EXCEPTION);

    document->updateLayout();
    unsigned startCount = document->styleEngine()->resolverAccessCount();

    document->documentElement()->renderer()->updateDragState(true);
    document->updateLayout();

    unsigned accessCount = document->styleEngine()->resolverAccessCount() - startCount;

    ASSERT_EQ(5U, accessCount);
}

} // namespace
