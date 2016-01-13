// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/context_menu_params.h"
#include "content/renderer/render_frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace content {

TEST(RenderFrameImplTest, ShouldUpdateSelectionTextFromContextMenuParams) {
  struct {
    const char* selection_text;
    size_t selection_text_offset;
    gfx::Range selection_range;
    const char* params_selection_text;
    bool expected_result;
  } cases[] = {
    { "test", 0, gfx::Range(0, 4), "test", false },
    { "zebestest", 0, gfx::Range(2, 6), "best", false },
    { "zebestest", 2, gfx::Range(2, 6), "best", true },
    { "test", 0, gfx::Range(0, 4), "hello", true },
    { "best test", 0, gfx::Range(0, 4), "best ", false },
    { "best test", 0, gfx::Range(0, 5), "best", false },
  };

  ContextMenuParams params;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    params.selection_text = base::UTF8ToUTF16(cases[i].params_selection_text);
    EXPECT_EQ(cases[i].expected_result,
              RenderFrameImpl::ShouldUpdateSelectionTextFromContextMenuParams(
                  base::UTF8ToUTF16(cases[i].selection_text),
                  cases[i].selection_text_offset,
                  cases[i].selection_range,
                  params));
  }
}

}  // namespace content
