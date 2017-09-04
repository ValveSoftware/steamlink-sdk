// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(DragUpdateTest, AffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you only get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(800, 600));
  Document& document = dummyPageHolder->document();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "</div>");

  document.view()->updateAllLifecyclePhases();
  unsigned startCount = document.styleEngine().styleForElementCount();

  document.getElementById("div")->setDragged(true);
  document.view()->updateAllLifecyclePhases();

  unsigned elementCount =
      document.styleEngine().styleForElementCount() - startCount;

  ASSERT_EQ(1U, elementCount);
}

TEST(DragUpdateTest, ChildAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(800, 600));
  Document& document = dummyPageHolder->document();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag .drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span class='drag'></span>"
      "<span></span>"
      "</div>");

  document.updateStyleAndLayout();
  unsigned startCount = document.styleEngine().styleForElementCount();

  document.getElementById("div")->setDragged(true);
  document.updateStyleAndLayout();

  unsigned elementCount =
      document.styleEngine().styleForElementCount() - startCount;

  ASSERT_EQ(1U, elementCount);
}

TEST(DragUpdateTest, SiblingAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(800, 600));
  Document& document = dummyPageHolder->document();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag + .drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "</div>"
      "<span class='drag'></span>");

  document.updateStyleAndLayout();
  unsigned startCount = document.styleEngine().styleForElementCount();

  document.getElementById("div")->setDragged(true);
  document.updateStyleAndLayout();

  unsigned elementCount =
      document.styleEngine().styleForElementCount() - startCount;

  ASSERT_EQ(1U, elementCount);
}

}  // namespace blink
