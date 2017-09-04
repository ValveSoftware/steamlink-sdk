// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLDocumentParser.h"

#include "core/html/HTMLDocument.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/PrerendererClient.h"
#include "core/loader/TextResourceDecoderBuilder.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

class TestPrerendererClient : public GarbageCollected<TestPrerendererClient>,
                              public PrerendererClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestPrerendererClient);

 public:
  TestPrerendererClient(bool isPrefetchOnly)
      : m_isPrefetchOnly(isPrefetchOnly) {}

 private:
  void willAddPrerender(Prerender*) override{};
  bool isPrefetchOnly() override { return m_isPrefetchOnly; }

  bool m_isPrefetchOnly;
};

class HTMLDocumentParserTest : public testing::Test {
 protected:
  HTMLDocumentParserTest() : m_dummyPageHolder(DummyPageHolder::create()) {
    m_dummyPageHolder->document().setURL(KURL(KURL(), "https://example.test"));
  }

  HTMLDocumentParser* createParser(HTMLDocument& document) {
    HTMLDocumentParser* parser =
        HTMLDocumentParser::create(document, ForceSynchronousParsing);
    TextResourceDecoderBuilder decoderBuilder("text/html", nullAtom);
    std::unique_ptr<TextResourceDecoder> decoder(
        decoderBuilder.buildFor(&document));
    parser->setDecoder(std::move(decoder));
    return parser;
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

}  // namespace

TEST_F(HTMLDocumentParserTest, AppendPrefetch) {
  HTMLDocument& document = toHTMLDocument(m_dummyPageHolder->document());
  providePrerendererClientTo(*document.page(), new TestPrerendererClient(true));
  EXPECT_TRUE(document.isPrefetchOnly());
  HTMLDocumentParser* parser = createParser(document);

  const char bytes[] = "<ht";
  parser->appendBytes(bytes, sizeof(bytes));
  // The bytes are forwarded to the preload scanner, not to the tokenizer.
  HTMLScriptRunnerHost* scriptRunnerHost =
      parser->asHTMLScriptRunnerHostForTesting();
  EXPECT_TRUE(scriptRunnerHost->hasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::DataState, parser->tokenizer()->getState());
}

TEST_F(HTMLDocumentParserTest, AppendNoPrefetch) {
  HTMLDocument& document = toHTMLDocument(m_dummyPageHolder->document());
  EXPECT_FALSE(document.isPrefetchOnly());
  // Use ForceSynchronousParsing to allow calling append().
  HTMLDocumentParser* parser = createParser(document);

  const char bytes[] = "<ht";
  parser->appendBytes(bytes, sizeof(bytes));
  // The bytes are forwarded to the tokenizer.
  HTMLScriptRunnerHost* scriptRunnerHost =
      parser->asHTMLScriptRunnerHostForTesting();
  EXPECT_FALSE(scriptRunnerHost->hasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::TagNameState, parser->tokenizer()->getState());
}

}  // namespace blink
