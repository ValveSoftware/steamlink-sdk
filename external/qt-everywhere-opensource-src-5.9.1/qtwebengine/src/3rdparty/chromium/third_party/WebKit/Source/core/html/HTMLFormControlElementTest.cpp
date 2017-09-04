// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLFormControlElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/ValidationMessageClient.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
class MockValidationMessageClient
    : public GarbageCollectedFinalized<MockValidationMessageClient>,
      public ValidationMessageClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockValidationMessageClient);

 public:
  void ShowValidationMessage(const Element& anchor,
                             const String&,
                             TextDirection,
                             const String&,
                             TextDirection) override {
    anchor_ = anchor;
  }

  void HideValidationMessage(const Element& anchor) override {
    if (anchor_ == &anchor)
      anchor_ = nullptr;
  }

  bool IsValidationMessageVisible(const Element& anchor) override {
    return anchor_ == &anchor;
  }

  void WillUnloadDocument(const Document&) override {}
  void DocumentDetached(const Document&) override {}
  void WillBeDestroyed() override {}
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(anchor_);
    ValidationMessageClient::Trace(visitor);
  }

 private:
  Member<const Element> anchor_;
};
}

class HTMLFormControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Page& page() const { return m_dummyPageHolder->page(); }
  Document& document() const { return *m_document; }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void HTMLFormControlElementTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

  m_document = &m_dummyPageHolder->document();
  m_document->setMimeType("text/html");
}

TEST_F(HTMLFormControlElementTest, customValidationMessageTextDirection) {
  document().documentElement()->setInnerHTML(
      "<body><input pattern='abc' value='def' id=input></body>",
      ASSERT_NO_EXCEPTION);
  document().view()->updateAllLifecyclePhases();

  HTMLInputElement* input =
      toHTMLInputElement(document().getElementById("input"));
  input->setCustomValidity(
      String::fromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));
  input->setAttribute(
      HTMLNames::titleAttr,
      AtomicString::fromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));

  String message = input->validationMessage().stripWhiteSpace();
  String subMessage = input->validationSubMessage().stripWhiteSpace();
  TextDirection messageDir = RTL;
  TextDirection subMessageDir = LTR;

  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(RTL, messageDir);
  EXPECT_EQ(LTR, subMessageDir);

  input->layoutObject()->mutableStyleRef().setDirection(RTL);
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(RTL, messageDir);
  EXPECT_EQ(LTR, subMessageDir);

  input->setCustomValidity(String::fromUTF8("Main message."));
  message = input->validationMessage().stripWhiteSpace();
  subMessage = input->validationSubMessage().stripWhiteSpace();
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(LTR, messageDir);
  EXPECT_EQ(LTR, subMessageDir);

  input->setCustomValidity(String());
  message = input->validationMessage().stripWhiteSpace();
  subMessage = input->validationSubMessage().stripWhiteSpace();
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(LTR, messageDir);
  EXPECT_EQ(RTL, subMessageDir);
}

TEST_F(HTMLFormControlElementTest, UpdateValidationMessageSkippedIfPrinting) {
  GetDocument().documentElement()->setInnerHTML(
      "<body><input required id=input></body>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ValidationMessageClient* validation_message_client =
      new MockValidationMessageClient();
  GetPage().SetValidationMessageClient(validation_message_client);
  Page::OrdinaryPages().insert(&GetPage());

  HTMLInputElement* input =
      toHTMLInputElement(GetDocument().getElementById("input"));
  ScopedPageSuspender suspender;  // print() suspends the page.
  input->reportValidity();
  EXPECT_FALSE(validation_message_client->IsValidationMessageVisible(*input));
}

}  // namespace blink
