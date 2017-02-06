// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/operators.h"

#include <stdint.h>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/pattern.h"
#include "tools/gn/test_with_scope.h"

namespace {

bool IsValueIntegerEqualing(const Value& v, int64_t i) {
  if (v.type() != Value::INTEGER)
    return false;
  return v.int_value() == i;
}

bool IsValueStringEqualing(const Value& v, const char* s) {
  if (v.type() != Value::STRING)
    return false;
  return v.string_value() == s;
}

// Returns a list populated with a single literal Value corresponding to the
// given token. The token must outlive the list (since the list will just
// copy the reference).
std::unique_ptr<ListNode> ListWithLiteral(const Token& token) {
  std::unique_ptr<ListNode> list(new ListNode);
  list->append_item(std::unique_ptr<ParseNode>(new LiteralNode(token)));
  return list;
}

}  // namespace

TEST(Operators, SourcesAppend) {
  Err err;
  TestWithScope setup;

  // Set up "sources" with an empty list.
  const char sources[] = "sources";
  setup.scope()->SetValue(sources, Value(nullptr, Value::LIST), nullptr);

  // Set up the operator.
  BinaryOpNode node;
  const char token_value[] = "+=";
  Token op(Location(), Token::PLUS_EQUALS, token_value);
  node.set_op(op);

  // Append to the sources variable.
  Token identifier_token(Location(), Token::IDENTIFIER, sources);
  node.set_left(
      std::unique_ptr<ParseNode>(new IdentifierNode(identifier_token)));

  // Set up the filter on the scope to remove everything ending with "rm"
  std::unique_ptr<PatternList> pattern_list(new PatternList);
  pattern_list->Append(Pattern("*rm"));
  setup.scope()->set_sources_assignment_filter(std::move(pattern_list));

  // Append an integer.
  const char integer_value[] = "5";
  Token integer(Location(), Token::INTEGER, integer_value);
  node.set_right(ListWithLiteral(integer));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a string that doesn't match the pattern, it should get appended.
  const char string_1_value[] = "\"good\"";
  Token string_1(Location(), Token::STRING, string_1_value);
  node.set_right(ListWithLiteral(string_1));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a string that does match the pattern, it should be a no-op.
  const char string_2_value[] = "\"foo-rm\"";
  Token string_2(Location(), Token::STRING, string_2_value);
  node.set_right(ListWithLiteral(string_2));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a list with the two strings from above.
  ListNode list;
  list.append_item(std::unique_ptr<ParseNode>(new LiteralNode(string_1)));
  list.append_item(std::unique_ptr<ParseNode>(new LiteralNode(string_2)));
  ExecuteBinaryOperator(setup.scope(), &node, node.left(), &list, &err);
  EXPECT_FALSE(err.has_error());

  // The sources variable in the scope should now have: [ 5, "good", "good" ]
  const Value* value = setup.scope()->GetValue(sources);
  ASSERT_TRUE(value);
  ASSERT_EQ(Value::LIST, value->type());
  ASSERT_EQ(3u, value->list_value().size());
  EXPECT_TRUE(IsValueIntegerEqualing(value->list_value()[0], 5));
  EXPECT_TRUE(IsValueStringEqualing(value->list_value()[1], "good"));
  EXPECT_TRUE(IsValueStringEqualing(value->list_value()[2], "good"));
}

// Note that the SourcesAppend test above tests the basic list + list features,
// this test handles the other cases.
TEST(Operators, ListAppend) {
  Err err;
  TestWithScope setup;

  // Set up "foo" with an empty list.
  const char foo[] = "foo";
  setup.scope()->SetValue(foo, Value(nullptr, Value::LIST), nullptr);

  // Set up the operator.
  BinaryOpNode node;
  const char token_value[] = "+=";
  Token op(Location(), Token::PLUS_EQUALS, token_value);
  node.set_op(op);

  // Append to the foo variable.
  Token identifier_token(Location(), Token::IDENTIFIER, foo);
  node.set_left(
      std::unique_ptr<ParseNode>(new IdentifierNode(identifier_token)));

  // Append a list with a list, the result should be a nested list.
  std::unique_ptr<ListNode> outer_list(new ListNode);
  const char twelve_str[] = "12";
  Token twelve(Location(), Token::INTEGER, twelve_str);
  outer_list->append_item(ListWithLiteral(twelve));
  node.set_right(std::move(outer_list));

  Value ret = ExecuteBinaryOperator(setup.scope(), &node, node.left(),
                                    node.right(), &err);
  EXPECT_FALSE(err.has_error());

  // Return from the operator should always be "none", it should update the
  // value only.
  EXPECT_EQ(Value::NONE, ret.type());

  // The value should be updated with "[ [ 12 ] ]"
  Value result = *setup.scope()->GetValue(foo);
  ASSERT_EQ(Value::LIST, result.type());
  ASSERT_EQ(1u, result.list_value().size());
  ASSERT_EQ(Value::LIST, result.list_value()[0].type());
  ASSERT_EQ(1u, result.list_value()[0].list_value().size());
  ASSERT_EQ(Value::INTEGER, result.list_value()[0].list_value()[0].type());
  ASSERT_EQ(12, result.list_value()[0].list_value()[0].int_value());

  // Try to append an integer and a string directly (e.g. foo += "hi").
  // This should fail.
  const char str_str[] = "\"hi\"";
  Token str(Location(), Token::STRING, str_str);
  node.set_right(std::unique_ptr<ParseNode>(new LiteralNode(str)));
  ExecuteBinaryOperator(setup.scope(), &node, node.left(), node.right(), &err);
  EXPECT_TRUE(err.has_error());
  err = Err();

  node.set_right(std::unique_ptr<ParseNode>(new LiteralNode(twelve)));
  ExecuteBinaryOperator(setup.scope(), &node, node.left(), node.right(), &err);
  EXPECT_TRUE(err.has_error());
}

TEST(Operators, ShortCircuitAnd) {
  Err err;
  TestWithScope setup;

  // Set up the operator.
  BinaryOpNode node;
  const char token_value[] = "&&";
  Token op(Location(), Token::BOOLEAN_AND, token_value);
  node.set_op(op);

  // Set the left to false.
  const char false_str[] = "false";
  Token false_tok(Location(), Token::FALSE_TOKEN, false_str);
  node.set_left(std::unique_ptr<ParseNode>(new LiteralNode(false_tok)));

  // Set right as foo, but don't define a value for it.
  const char foo[] = "foo";
  Token identifier_token(Location(), Token::IDENTIFIER, foo);
  node.set_right(
      std::unique_ptr<ParseNode>(new IdentifierNode(identifier_token)));

  Value ret = ExecuteBinaryOperator(setup.scope(), &node, node.left(),
                                    node.right(), &err);
  EXPECT_FALSE(err.has_error());
}

TEST(Operators, ShortCircuitOr) {
  Err err;
  TestWithScope setup;

  // Set up the operator.
  BinaryOpNode node;
  const char token_value[] = "||";
  Token op(Location(), Token::BOOLEAN_OR, token_value);
  node.set_op(op);

  // Set the left to false.
  const char false_str[] = "true";
  Token false_tok(Location(), Token::TRUE_TOKEN, false_str);
  node.set_left(std::unique_ptr<ParseNode>(new LiteralNode(false_tok)));

  // Set right as foo, but don't define a value for it.
  const char foo[] = "foo";
  Token identifier_token(Location(), Token::IDENTIFIER, foo);
  node.set_right(
      std::unique_ptr<ParseNode>(new IdentifierNode(identifier_token)));

  Value ret = ExecuteBinaryOperator(setup.scope(), &node, node.left(),
                                    node.right(), &err);
  EXPECT_FALSE(err.has_error());
}
