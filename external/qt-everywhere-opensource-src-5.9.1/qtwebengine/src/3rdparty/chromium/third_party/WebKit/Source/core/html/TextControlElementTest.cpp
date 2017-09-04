// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/TextControlElement.h"

#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/loader/EmptyClients.h"
#include "core/page/SpellCheckerClient.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class TextControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& page() const { return *m_dummyPageHolder; }
  Document& document() const { return *m_document; }
  TextControlElement& textControl() const { return *m_textControl; }
  HTMLInputElement& input() const { return *m_input; }

  int layoutCount() const { return page().frameView().layoutCount(); }
  void forceLayoutFlag();

 private:
  std::unique_ptr<SpellCheckerClient> m_spellCheckerClient;
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;

  Persistent<Document> m_document;
  Persistent<TextControlElement> m_textControl;
  Persistent<HTMLInputElement> m_input;
};

class DummySpellCheckerClient : public EmptySpellCheckerClient {
 public:
  virtual ~DummySpellCheckerClient() {}

  bool isSpellCheckingEnabled() override { return true; }

  TextCheckerClient& textChecker() override { return m_emptyTextCheckerClient; }

 private:
  EmptyTextCheckerClient m_emptyTextCheckerClient;
};

void TextControlElementTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_spellCheckerClient = wrapUnique(new DummySpellCheckerClient);
  pageClients.spellCheckerClient = m_spellCheckerClient.get();
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

  m_document = &m_dummyPageHolder->document();
  m_document->documentElement()->setInnerHTML(
      "<body><textarea id=textarea></textarea><input id=input /></body>");
  m_document->view()->updateAllLifecyclePhases();
  m_textControl = toTextControlElement(m_document->getElementById("textarea"));
  m_textControl->focus();
  m_input = toHTMLInputElement(m_document->getElementById("input"));
}

void TextControlElementTest::forceLayoutFlag() {
  FrameView& frameView = page().frameView();
  IntRect frameRect = frameView.frameRect();
  frameRect.setWidth(frameRect.width() + 1);
  frameRect.setHeight(frameRect.height() + 1);
  page().frameView().setFrameRect(frameRect);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
}

TEST_F(TextControlElementTest, SetSelectionRange) {
  EXPECT_EQ(0, textControl().selectionStart());
  EXPECT_EQ(0, textControl().selectionEnd());

  textControl().setInnerEditorValue("Hello, text form.");
  EXPECT_EQ(0, textControl().selectionStart());
  EXPECT_EQ(0, textControl().selectionEnd());

  textControl().setSelectionRange(1, 3);
  EXPECT_EQ(1, textControl().selectionStart());
  EXPECT_EQ(3, textControl().selectionEnd());
}

TEST_F(TextControlElementTest, SetSelectionRangeDoesNotCauseLayout) {
  input().focus();
  input().setValue("Hello, input form.");
  input().setSelectionRange(1, 1);
  FrameSelection& frameSelection = document().frame()->selection();
  forceLayoutFlag();
  LayoutRect oldCaretRect(frameSelection.absoluteCaretBounds());
  EXPECT_FALSE(oldCaretRect.isEmpty());
  int startLayoutCount = layoutCount();
  input().setSelectionRange(1, 1);
  EXPECT_EQ(startLayoutCount, layoutCount());
  LayoutRect newCaretRect(frameSelection.absoluteCaretBounds());
  EXPECT_EQ(oldCaretRect, newCaretRect);

  forceLayoutFlag();
  oldCaretRect = LayoutRect(frameSelection.absoluteCaretBounds());
  EXPECT_FALSE(oldCaretRect.isEmpty());
  startLayoutCount = layoutCount();
  input().setSelectionRange(2, 2);
  EXPECT_EQ(startLayoutCount, layoutCount());
  newCaretRect = LayoutRect(frameSelection.absoluteCaretBounds());
  EXPECT_NE(oldCaretRect, newCaretRect);
}

typedef Position (*PositionFunction)(const Position&);
typedef VisiblePosition (*VisblePositionFunction)(const VisiblePosition&);

void testFunctionEquivalence(const Position& position,
                             PositionFunction positionFunction,
                             VisblePositionFunction visibleFunction) {
  VisiblePosition visiblePosition = createVisiblePosition(position);
  VisiblePosition expected = visibleFunction(visiblePosition);
  VisiblePosition actual = createVisiblePosition(positionFunction(position));
  EXPECT_EQ(expected.deepEquivalent(), actual.deepEquivalent());
}

static VisiblePosition startOfWord(const VisiblePosition& position) {
  return startOfWord(position, LeftWordIfOnBoundary);
}

static VisiblePosition endOfWord(const VisiblePosition& position) {
  return endOfWord(position, RightWordIfOnBoundary);
}

void testBoundary(Document& document, TextControlElement& textControl) {
  document.updateStyleAndLayout();
  for (unsigned i = 0; i < textControl.innerEditorValue().length(); i++) {
    textControl.setSelectionRange(i, i);
    Position position = document.frame()->selection().start();
    SCOPED_TRACE(::testing::Message()
                 << "offset " << position.computeEditingOffset() << " of "
                 << nodePositionAsStringForTesting(position.anchorNode())
                        .ascii()
                        .data());
    {
      SCOPED_TRACE("TextControlElement::startOfWord");
      testFunctionEquivalence(position, TextControlElement::startOfWord,
                              startOfWord);
    }
    {
      SCOPED_TRACE("TextControlElement::endOfWord");
      testFunctionEquivalence(position, TextControlElement::endOfWord,
                              endOfWord);
    }
    {
      SCOPED_TRACE("TextControlElement::startOfSentence");
      testFunctionEquivalence(position, TextControlElement::startOfSentence,
                              startOfSentence);
    }
    {
      SCOPED_TRACE("TextControlElement::endOfSentence");
      testFunctionEquivalence(position, TextControlElement::endOfSentence,
                              endOfSentence);
    }
  }
}

TEST_F(TextControlElementTest, WordAndSentenceBoundary) {
  HTMLElement* innerText = textControl().innerEditorElement();
  {
    SCOPED_TRACE("String is value.");
    innerText->removeChildren();
    innerText->setNodeValue("Hel\nlo, text form.\n");
    testBoundary(document(), textControl());
  }
  {
    SCOPED_TRACE("A Text node and a BR element");
    innerText->removeChildren();
    innerText->setNodeValue("");
    innerText->appendChild(Text::create(document(), "Hello, text form."));
    innerText->appendChild(HTMLBRElement::create(document()));
    testBoundary(document(), textControl());
  }
  {
    SCOPED_TRACE("Text nodes.");
    innerText->removeChildren();
    innerText->setNodeValue("");
    innerText->appendChild(Text::create(document(), "Hel\nlo, te"));
    innerText->appendChild(Text::create(document(), "xt form."));
    testBoundary(document(), textControl());
  }
}

TEST_F(TextControlElementTest, IndexForPosition) {
  HTMLInputElement* input =
      toHTMLInputElement(document().getElementById("input"));
  input->setValue("Hello");
  HTMLElement* innerEditor = input->innerEditorElement();
  EXPECT_EQ(5, TextControlElement::indexForPosition(
                   innerEditor,
                   Position(innerEditor, PositionAnchorType::AfterAnchor)));
}

}  // namespace blink
