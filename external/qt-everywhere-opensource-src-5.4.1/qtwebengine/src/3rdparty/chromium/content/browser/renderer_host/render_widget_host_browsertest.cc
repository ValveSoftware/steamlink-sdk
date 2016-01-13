// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class RenderWidgetHostBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

 protected:
  base::FilePath test_dir_;
};

}  // namespace content
