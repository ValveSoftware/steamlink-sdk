// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleEngine.h"

#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class StyleEngineTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& document() { return m_dummyPageHolder->document(); }
  StyleEngine& styleEngine() { return document().styleEngine(); }

  bool isDocumentStyleSheetCollectionClean() {
    return !styleEngine().shouldUpdateDocumentStyleSheetCollection(
        AnalyzedStyleUpdate);
  }

  enum RuleSetInvalidation {
    RuleSetInvalidationsScheduled,
    RuleSetInvalidationFullRecalc
  };
  RuleSetInvalidation scheduleInvalidationsForRules(TreeScope&,
                                                    const String& cssText);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void StyleEngineTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

StyleEngineTest::RuleSetInvalidation
StyleEngineTest::scheduleInvalidationsForRules(TreeScope& treeScope,
                                               const String& cssText) {
  StyleSheetContents* sheet =
      StyleSheetContents::create(CSSParserContext(HTMLStandardMode, nullptr));
  sheet->parseString(cssText);
  HeapVector<Member<RuleSet>> ruleSets;
  RuleSet& ruleSet = sheet->ensureRuleSet(MediaQueryEvaluator(),
                                          RuleHasDocumentSecurityOrigin);
  ruleSet.compactRulesIfNeeded();
  if (ruleSet.needsFullRecalcForRuleSetInvalidation())
    return RuleSetInvalidationFullRecalc;
  ruleSets.append(&ruleSet);
  styleEngine().scheduleInvalidationsForRuleSets(treeScope, ruleSets);
  return RuleSetInvalidationsScheduled;
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject) {
  StyleSheetContents* parsedSheet =
      StyleSheetContents::create(CSSParserContext(document(), nullptr));
  parsedSheet->parseString("div {}");
  styleEngine().injectAuthorSheet(parsedSheet);
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(isDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject) {
  document().body()->setInnerHTML(
      "<style>div { color: red }</style><div id='t1'>Green</div><div></div>");
  document().view()->updateAllLifecyclePhases();

  Element* t1 = document().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->computedStyle());
  EXPECT_EQ(makeRGB(255, 0, 0),
            t1->computedStyle()->visitedDependentColor(CSSPropertyColor));

  unsigned beforeCount = styleEngine().styleForElementCount();

  StyleSheetContents* parsedSheet =
      StyleSheetContents::create(CSSParserContext(document(), nullptr));
  parsedSheet->parseString("#t1 { color: green }");
  styleEngine().injectAuthorSheet(parsedSheet);
  document().view()->updateAllLifecyclePhases();

  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(1u, afterCount - beforeCount);

  ASSERT_TRUE(t1->computedStyle());
  EXPECT_EQ(makeRGB(0, 128, 0),
            t1->computedStyle()->visitedDependentColor(CSSPropertyColor));
}

TEST_F(StyleEngineTest, TextToSheetCache) {
  HTMLStyleElement* element = HTMLStyleElement::create(document(), false);

  String sheetText("div {}");
  TextPosition minPos = TextPosition::minimumPosition();
  StyleEngineContext context;

  CSSStyleSheet* sheet1 =
      styleEngine().createSheet(*element, sheetText, minPos, context);

  // Check that the first sheet is not using a cached StyleSheetContents.
  EXPECT_FALSE(sheet1->contents()->isUsedFromTextCache());

  CSSStyleSheet* sheet2 =
      styleEngine().createSheet(*element, sheetText, minPos, context);

  // Check that the second sheet uses the cached StyleSheetContents for the
  // first.
  EXPECT_EQ(sheet1->contents(), sheet2->contents());
  EXPECT_TRUE(sheet2->contents()->isUsedFromTextCache());

  sheet1 = nullptr;
  sheet2 = nullptr;
  element = nullptr;

  // Garbage collection should clear the weak reference in the
  // StyleSheetContents cache.
  ThreadState::current()->collectAllGarbage();

  element = HTMLStyleElement::create(document(), false);
  sheet1 = styleEngine().createSheet(*element, sheetText, minPos, context);

  // Check that we did not use a cached StyleSheetContents after the garbage
  // collection.
  EXPECT_FALSE(sheet1->contents()->isUsedFromTextCache());
}

TEST_F(StyleEngineTest, RuleSetInvalidationTypeSelectors) {
  document().body()->setInnerHTML(
      "<div>"
      "  <span></span>"
      "  <div></div>"
      "</div>");

  document().view()->updateAllLifecyclePhases();

  unsigned beforeCount = styleEngine().styleForElementCount();
  EXPECT_EQ(
      scheduleInvalidationsForRules(document(), "span { background: green}"),
      RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(1u, afterCount - beforeCount);

  beforeCount = afterCount;
  EXPECT_EQ(scheduleInvalidationsForRules(document(),
                                          "body div { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(2u, afterCount - beforeCount);

  EXPECT_EQ(
      scheduleInvalidationsForRules(document(), "div * { background: green}"),
      RuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHost) {
  document().body()->setInnerHTML("<div id=nohost></div><div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host->attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadowRoot);

  shadowRoot->setInnerHTML("<div></div><div></div><div></div>");
  document().view()->updateAllLifecyclePhases();

  unsigned beforeCount = styleEngine().styleForElementCount();
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ":host(#nohost), #nohost { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(0u, afterCount - beforeCount);

  beforeCount = afterCount;
  EXPECT_EQ(scheduleInvalidationsForRules(*shadowRoot,
                                          ":host(#host) { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(1u, afterCount - beforeCount);

  EXPECT_EQ(scheduleInvalidationsForRules(*shadowRoot,
                                          ":host(*) { background: green}"),
            RuleSetInvalidationFullRecalc);
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ":host(*) :hover { background: green}"),
            RuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationSlotted) {
  document().body()->setInnerHTML(
      "<div id=host>"
      "  <span slot=other class=s1></span>"
      "  <span class=s2></span>"
      "  <span class=s1></span>"
      "  <span></span>"
      "</div>");

  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host->attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadowRoot);

  shadowRoot->setInnerHTML("<slot name=other></slot><slot></slot>");
  document().view()->updateAllLifecyclePhases();

  unsigned beforeCount = styleEngine().styleForElementCount();
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, "::slotted(.s1) { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(4u, afterCount - beforeCount);

  beforeCount = afterCount;
  EXPECT_EQ(scheduleInvalidationsForRules(*shadowRoot,
                                          "::slotted(*) { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(4u, afterCount - beforeCount);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHostContext) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host->attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadowRoot);

  shadowRoot->setInnerHTML("<div></div><div class=a></div><div></div>");
  document().view()->updateAllLifecyclePhases();

  unsigned beforeCount = styleEngine().styleForElementCount();
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ":host-context(.nomatch) .a { background: green}"),
            RuleSetInvalidationsScheduled);
  document().view()->updateAllLifecyclePhases();
  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(1u, afterCount - beforeCount);

  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ":host-context(:hover) { background: green}"),
            RuleSetInvalidationFullRecalc);
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ":host-context(#host) { background: green}"),
            RuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationV0BoundaryCrossing) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host->attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadowRoot);

  shadowRoot->setInnerHTML("<div></div><div class=a></div><div></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ".a ::content span { background: green}"),
            RuleSetInvalidationFullRecalc);
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ".a /deep/ span { background: green}"),
            RuleSetInvalidationFullRecalc);
  EXPECT_EQ(scheduleInvalidationsForRules(
                *shadowRoot, ".a::shadow span { background: green}"),
            RuleSetInvalidationFullRecalc);
}

}  // namespace blink
