// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLOutputElement.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLLinkElementSizesAttributeTest, setHTMLForProperty_updatesForAttribute)
{
    Document* document = Document::create();
    HTMLOutputElement* element = HTMLOutputElement::create(*document, /* form: */ nullptr);
    EXPECT_EQ(nullAtom, element->getAttribute(HTMLNames::forAttr));
    element->htmlFor()->setValue("  strawberry ");
    EXPECT_EQ("  strawberry ", element->getAttribute(HTMLNames::forAttr));
}

TEST(HTMLOutputElementTest, setForAttribute_updatesHTMLForPropertyValue)
{
    Document* document = Document::create();
    HTMLOutputElement* element = HTMLOutputElement::create(*document, nullptr);
    DOMTokenList* forTokens = element->htmlFor();
    EXPECT_EQ(nullAtom, forTokens->value());
    element->setAttribute(HTMLNames::forAttr, "orange grape");
    EXPECT_EQ("orange grape", forTokens->value());
}

} // namespace blink
