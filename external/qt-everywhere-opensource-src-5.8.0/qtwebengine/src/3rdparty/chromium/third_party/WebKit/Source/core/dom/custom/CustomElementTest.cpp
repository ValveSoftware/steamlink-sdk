// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

static void testIsPotentialCustomElementName(const AtomicString& str, bool expected)
{
    if (expected) {
        EXPECT_TRUE(CustomElement::isValidName(str))
            << str << " should be a valid custom element name.";
    } else {
        EXPECT_FALSE(CustomElement::isValidName(str))
            << str << " should NOT be a valid custom element name.";
    }
}

static void testIsPotentialCustomElementNameChar(UChar32 c, bool expected)
{
    LChar str8[] = "a-X";
    UChar str16[] = { 'a', '-', 'X', '\0', '\0' };
    AtomicString str;
    if (c <= 0xFF) {
        str8[2] = c;
        str = str8;
    } else {
        size_t i = 2;
        U16_APPEND_UNSAFE(str16, i, c);
        str16[i] = 0;
        str = str16;
    }
    testIsPotentialCustomElementName(str, expected);
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementName)
{
    struct {
        bool expected;
        AtomicString str;
    } tests[] = {
        { false, "" },
        { false, "a" },
        { false, "A" },

        { false, "A-" },
        { false, "0-" },

        { true, "a-" },
        { true, "a-a" },
        { true, "aa-" },
        { true, "aa-a" },
        { true, reinterpret_cast<const UChar*>(u"aa-\x6F22\x5B57") }, // Two CJK Unified Ideographs
        { true, reinterpret_cast<const UChar*>(u"aa-\xD840\xDC0B") }, // Surrogate pair U+2000B

        { false, "a-A" },
        { false, "a-Z" },
    };
    for (auto test : tests)
        testIsPotentialCustomElementName(test.str, test.expected);
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameChar)
{
    struct {
        UChar32 from, to;
    } ranges[] = {
        { '-', '.' }, // "-" | "." need to merge to test -1/+1.
        { '0', '9' },
        { '_', '_' },
        { 'a', 'z' },
        { 0xB7, 0xB7 },
        { 0xC0, 0xD6 },
        { 0xD8, 0xF6 },
        { 0xF8, 0x37D }, // [#xF8-#x2FF] | [#x300-#x37D] need to merge to test -1/+1.
        { 0x37F, 0x1FFF },
        { 0x200C, 0x200D },
        { 0x203F, 0x2040 },
        { 0x2070, 0x218F },
        { 0x2C00, 0x2FEF },
        { 0x3001, 0xD7FF },
        { 0xF900, 0xFDCF },
        { 0xFDF0, 0xFFFD },
        { 0x10000, 0xEFFFF },
    };
    for (auto range : ranges) {
        testIsPotentialCustomElementNameChar(range.from - 1, false);
        for (UChar32 c = range.from; c <= range.to; ++c)
            testIsPotentialCustomElementNameChar(c, true);
        testIsPotentialCustomElementNameChar(range.to + 1, false);
    }
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameCharFalse)
{
    struct {
        UChar32 from, to;
    } ranges[] = {
        { 'A', 'Z' },
    };
    for (auto range : ranges) {
        for (UChar32 c = range.from; c <= range.to; ++c)
            testIsPotentialCustomElementNameChar(c, false);
    }
}

TEST(CustomElementTest, TestIsValidNameHyphenContainingElementNames)
{
    EXPECT_TRUE(CustomElement::isValidName("valid-name"));

    EXPECT_FALSE(CustomElement::isValidName("annotation-xml"));
    EXPECT_FALSE(CustomElement::isValidName("color-profile"));
    EXPECT_FALSE(CustomElement::isValidName("font-face"));
    EXPECT_FALSE(CustomElement::isValidName("font-face-src"));
    EXPECT_FALSE(CustomElement::isValidName("font-face-uri"));
    EXPECT_FALSE(CustomElement::isValidName("font-face-format"));
    EXPECT_FALSE(CustomElement::isValidName("font-face-name"));
    EXPECT_FALSE(CustomElement::isValidName("missing-glyph"));
}

TEST(CustomElementTest, StateByParser)
{
    const char* bodyContent = "<div id=div></div>"
        "<a-a id=v1v0></a-a>"
        "<font-face id=v0></font-face>";
    std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
    Document& document = pageHolder->document();
    document.body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);

    struct {
        const char* id;
        CustomElementState state;
        Element::V0CustomElementState v0state;
    } parserData[] = {
        { "div", CustomElementState::Uncustomized, Element::V0NotCustomElement },
        { "v1v0", CustomElementState::Undefined, Element::V0WaitingForUpgrade },
        { "v0", CustomElementState::Uncustomized, Element::V0WaitingForUpgrade },
    };
    for (const auto& data : parserData) {
        Element* element = document.getElementById(data.id);
        EXPECT_EQ(data.state, element->getCustomElementState()) << data.id;
        EXPECT_EQ(data.v0state, element->getV0CustomElementState()) << data.id;
    }
}

TEST(CustomElementTest, StateByCreateElement)
{
    struct {
        const char* name;
        CustomElementState state;
        Element::V0CustomElementState v0state;
    } createElementData[] = {
        { "div", CustomElementState::Uncustomized, Element::V0NotCustomElement },
        { "a-a", CustomElementState::Undefined, Element::V0WaitingForUpgrade },
        // TODO(pdr): <font-face> should be V0NotCustomElement as per the spec,
        // but was regressed to be V0WaitingForUpgrade in
        // http://crrev.com/656913006
        { "font-face", CustomElementState::Uncustomized, Element::V0WaitingForUpgrade },
        { "_-X", CustomElementState::Uncustomized, Element::V0WaitingForUpgrade },
    };
    std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
    Document& document = pageHolder->document();
    for (const auto& data : createElementData) {
        Element* element = document.createElement(data.name, ASSERT_NO_EXCEPTION);
        EXPECT_EQ(data.state, element->getCustomElementState()) << data.name;
        EXPECT_EQ(data.v0state, element->getV0CustomElementState()) << data.name;

        element = document.createElementNS(HTMLNames::xhtmlNamespaceURI, data.name, ASSERT_NO_EXCEPTION);
        EXPECT_EQ(data.state, element->getCustomElementState()) << data.name;
        EXPECT_EQ(data.v0state, element->getV0CustomElementState()) << data.name;

        element = document.createElementNS(SVGNames::svgNamespaceURI, data.name, ASSERT_NO_EXCEPTION);
        EXPECT_EQ(CustomElementState::Uncustomized, element->getCustomElementState()) << data.name;
        EXPECT_EQ(data.v0state, element->getV0CustomElementState()) << data.name;
    }
}

} // namespace blink
