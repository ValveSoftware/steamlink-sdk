// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/json/JSONValues.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ObjectPaintInvalidatorTest = RenderingTest;

TEST_F(ObjectPaintInvalidatorTest,
       TraverseNonCompositingDescendantsInPaintOrder) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<style>div { width: 10px; height: 10px; background-color: green; "
      "}</style>"
      "<div id='container' style='position: fixed'>"
      "  <div id='normal-child'></div>"
      "  <div id='stacked-child' style='position: relative'></div>"
      "  <div id='composited-stacking-context' style='will-change: transform'>"
      "    <div id='normal-child-of-composited-stacking-context'></div>"
      "    <div id='stacked-child-of-composited-stacking-context' "
      "style='position: relative'></div>"
      "  </div>"
      "  <div id='composited-non-stacking-context' style='backface-visibility: "
      "hidden'>"
      "    <div id='normal-child-of-composited-non-stacking-context'></div>"
      "    <div id='stacked-child-of-composited-non-stacking-context' "
      "style='position: relative'></div>"
      "    <div "
      "id='non-stacked-layered-child-of-composited-non-stacking-context' "
      "style='overflow: scroll'></div>"
      "  </div>"
      "</div>");

  document().view()->setTracksPaintInvalidations(true);
  ObjectPaintInvalidator(*getLayoutObjectByElementId("container"))
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  std::unique_ptr<JSONArray> invalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  document().view()->setTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("container")->debugName(), s);
  JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("normal-child")->debugName(), s);
  JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("stacked-child")->debugName(), s);
  JSONObject::cast(invalidations->at(3))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId(
                "stacked-child-of-composited-non-stacking-context")
                ->debugName(),
            s);
}

}  // namespace blink
