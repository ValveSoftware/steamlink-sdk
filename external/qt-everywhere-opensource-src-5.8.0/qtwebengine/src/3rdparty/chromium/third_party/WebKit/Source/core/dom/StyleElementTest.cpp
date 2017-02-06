// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleElement.h"

#include "core/css/StyleSheetContents.h"
#include "core/dom/Comment.h"
#include "core/html/HTMLStyleElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(StyleElementTest, CreateSheetUsesCache)
{
    std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    Document& document = dummyPageHolder->document();

    document.documentElement()->setInnerHTML("<style id=style>a { top: 0; }</style>", ASSERT_NO_EXCEPTION);

    HTMLStyleElement& styleElement = toHTMLStyleElement(*document.getElementById("style"));
    StyleSheetContents* sheet = styleElement.sheet()->contents();

    Comment* comment = document.createComment("hello!");
    styleElement.appendChild(comment);
    EXPECT_EQ(styleElement.sheet()->contents(), sheet);

    styleElement.removeChild(comment);
    EXPECT_EQ(styleElement.sheet()->contents(), sheet);
}

} // namespace blink
