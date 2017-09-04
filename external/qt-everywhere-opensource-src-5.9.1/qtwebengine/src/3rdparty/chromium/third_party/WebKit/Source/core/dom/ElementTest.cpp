// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Element.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(ElementTest, SupportsFocus) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();
  DCHECK(isHTMLHtmlElement(document.documentElement()));
  document.setDesignMode("on");
  document.view()->updateAllLifecyclePhases();
  EXPECT_TRUE(document.documentElement()->supportsFocus())
      << "<html> with designMode=on should be focusable.";
}

}  // namespace blink
