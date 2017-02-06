// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBox.h"

#include "core/html/HTMLElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBoxTest : public RenderingTest {
};

TEST_F(LayoutBoxTest, BackgroundObscuredInRect)
{
    setBodyInnerHTML("<style>.column { width: 295.4px; padding-left: 10.4px; } .white-background { background: red; position: relative; overflow: hidden; border-radius: 1px; }"
        ".black-background { height: 100px; background: black; color: white; } </style>"
        "<div class='column'> <div> <div id='target' class='white-background'> <div class='black-background'></div> </div> </div> </div>");
    LayoutObject* layoutObject = getLayoutObjectByElementId("target");
    ASSERT_TRUE(layoutObject);
    ASSERT_TRUE(layoutObject->boxDecorationBackgroundIsKnownToBeObscured());
}

} // namespace blink
