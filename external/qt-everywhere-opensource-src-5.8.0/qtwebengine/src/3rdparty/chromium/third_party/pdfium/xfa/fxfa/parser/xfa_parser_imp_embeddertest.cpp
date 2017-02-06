// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class XFAParserImpEmbeddertest : public EmbedderTest {};

TEST_F(XFAParserImpEmbeddertest, Bug_216) {
  EXPECT_TRUE(OpenDocument("bug_216.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  UnloadPage(page);
}
