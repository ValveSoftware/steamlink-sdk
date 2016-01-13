// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/src/utils/debugger/SkDebugCanvas.h"
#include "third_party/skia/src/utils/debugger/SkDrawCommand.h"

namespace {

testing::AssertionResult HasInfoField(SkDebugCanvas& canvas, int index,
                                      const char* field) {

  SkTDArray<SkString*>* info = canvas.getCommandInfo(index);
  if (info == NULL)
    return testing::AssertionFailure() << " command info not found for index "
                                       << index;

  for (int i = 0; i < info->count(); ++i) {
    const SkString* info_str = (*info)[i];
    if (info_str == NULL)
      return testing::AssertionFailure() << " NULL info string for index "
                                         << index;

    // FIXME: loose info paramter test.
    if (strstr(info_str->c_str(), field) != NULL)
      return testing::AssertionSuccess() << field << " found";
  }

  return testing::AssertionFailure() << field << " not found";
}

}

namespace content {

TEST(SkiaBenchmarkingExtensionTest, SkDebugCanvas) {
  SkGraphics::Init();

  // Prepare canvas and resources.
  SkDebugCanvas canvas(100, 100);
  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkRect fullRect = SkRect::MakeWH(SkIntToScalar(100), SkIntToScalar(100));
  SkRect fillRect = SkRect::MakeXYWH(SkIntToScalar(25), SkIntToScalar(25),
                                     SkIntToScalar(50), SkIntToScalar(50));

  // Draw a trivial scene.
  canvas.save();
  canvas.clipRect(fullRect, SkRegion::kIntersect_Op, false);
  canvas.translate(SkIntToScalar(10), SkIntToScalar(10));
  canvas.scale(SkIntToScalar(2), SkIntToScalar(2));
  canvas.drawRect(fillRect, red_paint);
  canvas.restore();

  // Verify the recorded commands.
  DrawType cmd;
  int idx = 0;
  ASSERT_EQ(canvas.getSize(), 6);

  ASSERT_TRUE(canvas.getDrawCommandAt(idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SAVE);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Save");

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, CLIP_RECT);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Clip Rect");
  EXPECT_TRUE(HasInfoField(canvas, idx, "SkRect"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "Op"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "doAA"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, TRANSLATE);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Translate");
  EXPECT_TRUE(HasInfoField(canvas, idx, "dx"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "dy"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, SCALE);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Scale");
  EXPECT_TRUE(HasInfoField(canvas, idx, "sx"));
  EXPECT_TRUE(HasInfoField(canvas, idx, "sy"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, DRAW_RECT);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Draw Rect");
  EXPECT_TRUE(HasInfoField(canvas, idx, "SkRect"));

  ASSERT_TRUE(canvas.getDrawCommandAt(++idx) != NULL);
  cmd = canvas.getDrawCommandAt(idx)->getType();
  EXPECT_EQ(cmd, RESTORE);
  EXPECT_STREQ(SkDrawCommand::GetCommandString(cmd), "Restore");
}

} // namespace content
