// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElement.h"

#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLIFrameElementTest, SetPermissionsAttribute)
{
    Document* document = Document::create();
    HTMLIFrameElement* iframe = HTMLIFrameElement::create(*document);

    // Test setting via the Element attribute (HTML codepath).
    iframe->setAttribute(HTMLNames::permissionsAttr, "geolocation");
    EXPECT_EQ("geolocation", iframe->permissions()->value());
    iframe->setAttribute(HTMLNames::permissionsAttr, "geolocation notifications");
    EXPECT_EQ("geolocation notifications", iframe->permissions()->value());

    // Test setting via the DOMTokenList (JS codepath).
    iframe->permissions()->setValue("midi");
    EXPECT_EQ("midi", iframe->getAttribute(HTMLNames::permissionsAttr));
}

} // namespace blink
