// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLLinkElement.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLLinkElementSizesAttributeTest : public testing::Test {};

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesPropertyValue_updatesAttribute) {
  Document* document = Document::create();
  HTMLLinkElement* link =
      HTMLLinkElement::create(*document, /* createdByParser: */ false);
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(nullAtom, sizes->value());
  sizes->setValue("   a b  c ");
  EXPECT_EQ("   a b  c ", link->getAttribute(HTMLNames::sizesAttr));
  EXPECT_EQ("   a b  c ", sizes->value());
}

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesAttribute_updatesSizesPropertyValue) {
  Document* document = Document::create();
  HTMLLinkElement* link =
      HTMLLinkElement::create(*document, /* createdByParser: */ false);
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(nullAtom, sizes->value());
  link->setAttribute(HTMLNames::sizesAttr, "y  x ");
  EXPECT_EQ("y  x ", sizes->value());
}

}  // namespace blink
