// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLInputElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(HTMLInputElementTest, create)
{
    Document* document = Document::create();
    HTMLInputElement* input = HTMLInputElement::create(*document, nullptr, /* createdByParser */ false);
    EXPECT_NE(nullptr, input->userAgentShadowRoot());

    input = HTMLInputElement::create(*document, nullptr, /* createdByParser */ true);
    EXPECT_EQ(nullptr, input->userAgentShadowRoot());
    input->parserSetAttributes(Vector<Attribute>());
    EXPECT_NE(nullptr, input->userAgentShadowRoot());
}

TEST(HTMLInputElementTest, NoAssertWhenMovedInNewDocument)
{
    Document* documentWithoutFrame = Document::create();
    EXPECT_EQ(nullptr, documentWithoutFrame->frameHost());
    HTMLHtmlElement* html = HTMLHtmlElement::create(*documentWithoutFrame);
    html->appendChild(HTMLBodyElement::create(*documentWithoutFrame));

    // Create an input element with type "range" inside a document without frame.
    toHTMLBodyElement(html->firstChild())->setInnerHTML("<input type='range' />", ASSERT_NO_EXCEPTION);
    documentWithoutFrame->appendChild(html);

    std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
    auto& document = pageHolder->document();
    EXPECT_NE(nullptr, document.frameHost());

    // Put the input element inside a document with frame.
    document.body()->appendChild(documentWithoutFrame->body()->firstChild());

    // Remove the input element and all refs to it so it gets deleted before the document.
    // The assert in |EventHandlerRegistry::updateEventHandlerTargets()| should not be triggered.
    document.body()->removeChild(document.body()->firstChild());
}

TEST(HTMLInputElementTest, DefaultToolTip)
{
    Document* document = Document::create();
    HTMLHtmlElement* html = HTMLHtmlElement::create(*document);
    html->appendChild(HTMLBodyElement::create(*document));
    HTMLInputElement* inputWithoutForm = HTMLInputElement::create(*document, nullptr, false);
    inputWithoutForm->setBooleanAttribute(HTMLNames::requiredAttr, true);
    toHTMLBodyElement(html->firstChild())->appendChild(inputWithoutForm);
    document->appendChild(html);
    EXPECT_EQ("<<ValidationValueMissing>>", inputWithoutForm->defaultToolTip());

    HTMLFormElement* form = HTMLFormElement::create(*document);
    document->body()->appendChild(form);
    HTMLInputElement* inputWithForm = HTMLInputElement::create(*document, nullptr, false);
    inputWithForm->setBooleanAttribute(HTMLNames::requiredAttr, true);
    form->appendChild(inputWithForm);
    EXPECT_EQ("<<ValidationValueMissing>>", inputWithForm->defaultToolTip());

    form->setBooleanAttribute(HTMLNames::novalidateAttr, true);
    EXPECT_EQ(String(), inputWithForm->defaultToolTip());
}

// crbug.com/589838
TEST(HTMLInputElementTest, ImageTypeCrash)
{
    Document* document = Document::create();
    HTMLInputElement* input = HTMLInputElement::create(*document, nullptr, false);
    input->setAttribute(HTMLNames::typeAttr, "image");
    input->ensureFallbackContent();
    // Make sure ensurePrimaryContent() recreates UA shadow tree, and updating
    // |value| doesn't crash.
    input->ensurePrimaryContent();
    input->setAttribute(HTMLNames::valueAttr, "aaa");
}

TEST(HTMLInputElementTest, DateTimeChooserSizeParamRespectsScale)
{
    std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
    Document* document = &(pageHolder->document());
    document->view()->frame().host()->visualViewport().setScale(2.f);
    document->body()->setInnerHTML("<input type='date' style='width:200px;height:50px' />", ASSERT_NO_EXCEPTION);
    document->view()->updateAllLifecyclePhases();
    HTMLInputElement* input = toHTMLInputElement(document->body()->firstChild());

    DateTimeChooserParameters params;
    bool success = input->setupDateTimeChooserParameters(params);
    EXPECT_TRUE(success);
    EXPECT_EQ("date", params.type);
    EXPECT_EQ(IntRect(16, 16, 400, 100), params.anchorRectInScreen);
}

} // namespace blink
