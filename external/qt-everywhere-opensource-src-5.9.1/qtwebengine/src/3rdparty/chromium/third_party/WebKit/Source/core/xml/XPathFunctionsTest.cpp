// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/XPathFunctions.h"

#include "core/dom/Document.h"
#include "core/xml/XPathExpressionNode.h"  // EvaluationContext
#include "core/xml/XPathPredicate.h"       // Number, StringExpression
#include "core/xml/XPathValue.h"
#include "platform/heap/Handle.h"  // HeapVector, Member, etc.
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Allocator.h"

#include <cmath>
#include <limits>

namespace blink {

namespace {

class XPathContext {
  STACK_ALLOCATED();

 public:
  XPathContext() : m_document(Document::create()), m_context(*m_document) {}

  XPath::EvaluationContext& context() { return m_context; }
  Document& document() { return *m_document; }

 private:
  const Member<Document> m_document;
  XPath::EvaluationContext m_context;
};

using XPathArguments = HeapVector<Member<XPath::Expression>>;

static String substring(XPathArguments& args) {
  XPathContext xpath;
  XPath::Expression* call = XPath::createFunction("substring", args);
  XPath::Value result = call->evaluate(xpath.context());
  return result.toString();
}

static String substring(const char* string, double pos) {
  XPathArguments args;
  args.append(new XPath::StringExpression(string));
  args.append(new XPath::Number(pos));
  return substring(args);
}

static String substring(const char* string, double pos, double len) {
  XPathArguments args;
  args.append(new XPath::StringExpression(string));
  args.append(new XPath::Number(pos));
  args.append(new XPath::Number(len));
  return substring(args);
}

}  // namespace

TEST(XPathFunctionsTest, substring_specExamples) {
  EXPECT_EQ(" car", substring("motor car", 6.0))
      << "should select characters staring at position 6 to the end";
  EXPECT_EQ("ada", substring("metadata", 4.0, 3.0))
      << "should select characters at 4 <= position < 7";
  EXPECT_EQ("234", substring("123456", 1.5, 2.6))
      << "should select characters at 2 <= position < 5";
  EXPECT_EQ("12", substring("12345", 0.0, 3.0))
      << "should select characters at 0 <= position < 3; note the first "
         "position is 1 so this is characters in position 1 and 2";
  EXPECT_EQ("", substring("12345", 5.0, -3.0))
      << "no characters should have 5 <= position < 2";
  EXPECT_EQ("1", substring("12345", -3.0, 5.0))
      << "should select characters at -3 <= position < 2; since the first "
         "position is 1, this is the character at position 1";
  EXPECT_EQ("", substring("12345", NAN, 3.0))
      << "should select no characters since NaN <= position is always false";
  EXPECT_EQ("", substring("12345", 1.0, NAN))
      << "should select no characters since position < 1. + NaN is always "
         "false";
  EXPECT_EQ("12345",
            substring("12345", -42, std::numeric_limits<double>::infinity()))
      << "should select characters at -42 <= position < Infinity, which is all "
         "of them";
  EXPECT_EQ("", substring("12345", -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()))
      << "since -Inf+Inf is NaN, should select no characters since position "
         "< NaN is always false";
}

TEST(XPathFunctionsTest, substring_emptyString) {
  EXPECT_EQ("", substring("", 0.0, 1.0))
      << "substring of an empty string should be the empty string";
}

TEST(XPathFunctionsTest, substring) {
  EXPECT_EQ("hello", substring("well hello there", 6.0, 5.0));
}

TEST(XPathFunctionsTest, substring_negativePosition) {
  EXPECT_EQ("hello", substring("hello, world!", -4.0, 10.0))
      << "negative start positions should impinge on the result length";
  // Try to underflow the length adjustment for negative positions.
  EXPECT_EQ("", substring("hello", std::numeric_limits<long>::min() + 1, 1.0));
}

TEST(XPathFunctionsTest, substring_negativeLength) {
  EXPECT_EQ("", substring("hello, world!", 1.0, -3.0))
      << "negative lengths should result in an empty string";

  EXPECT_EQ("", substring("foo", std::numeric_limits<long>::min(), 1.0))
      << "large (but long representable) negative position should result in "
      << "an empty string";
}

TEST(XPathFunctionsTest, substring_extremePositionLength) {
  EXPECT_EQ("", substring("no way", 1e100, 7.0))
      << "extremely large positions should result in the empty string";

  EXPECT_EQ("no way", substring("no way", -1e200, 1e300))
      << "although these indices are not representable as long, this should "
      << "produce the string because indices are computed as doubles";
}

}  // namespace blink
