// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/operators.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

const char kSourcesName[] = "sources";

// Applies the sources assignment filter from the given scope to each element
// of source (can be a list or a string), appending it to dest if it doesn't
// match.
void AppendFilteredSourcesToValue(const Scope* scope,
                                  const Value& source,
                                  Value* dest) {
  const PatternList* filter = scope->GetSourcesAssignmentFilter();

  if (source.type() == Value::STRING) {
    if (!filter || filter->is_empty() ||
        !filter->MatchesValue(source))
      dest->list_value().push_back(source);
    return;
  }
  if (source.type() != Value::LIST) {
    // Any non-list and non-string being added to a list can just get appended,
    // we're not going to filter it.
    dest->list_value().push_back(source);
    return;
  }

  if (!filter || filter->is_empty()) {
    // No filter, append everything.
    for (const auto& source_entry : source.list_value())
      dest->list_value().push_back(source_entry);
    return;
  }

  // Note: don't reserve() the dest vector here since that actually hurts
  // the allocation pattern when the build script is doing multiple small
  // additions.
  for (const auto& source_entry : source.list_value()) {
    if (!filter->MatchesValue(source_entry))
      dest->list_value().push_back(source_entry);
  }
}

Value GetValueOrFillError(const BinaryOpNode* op_node,
                          const ParseNode* node,
                          const char* name,
                          Scope* scope,
                          Err* err) {
  Value value = node->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (value.type() == Value::NONE) {
    *err = Err(op_node->op(),
               "Operator requires a value.",
               "This thing on the " + std::string(name) +
                   " does not evaluate to a value.");
    err->AppendRange(node->GetRange());
    return Value();
  }
  return value;
}

void RemoveMatchesFromList(const BinaryOpNode* op_node,
                           Value* list,
                           const Value& to_remove,
                           Err* err) {
  std::vector<Value>& v = list->list_value();
  switch (to_remove.type()) {
    case Value::BOOLEAN:
    case Value::INTEGER:  // Filter out the individual int/string.
    case Value::STRING: {
      bool found_match = false;
      for (size_t i = 0; i < v.size(); /* nothing */) {
        if (v[i] == to_remove) {
          found_match = true;
          v.erase(v.begin() + i);
        } else {
          i++;
        }
      }
      if (!found_match) {
        *err = Err(to_remove.origin()->GetRange(), "Item not found",
            "You were trying to remove " + to_remove.ToString(true) +
            "\nfrom the list but it wasn't there.");
      }
      break;
    }

    case Value::LIST:  // Filter out each individual thing.
      for (const auto& elem : to_remove.list_value()) {
        // TODO(brettw) if the nested item is a list, we may want to search
        // for the literal list rather than remote the items in it.
        RemoveMatchesFromList(op_node, list, elem, err);
        if (err->has_error())
          return;
      }
      break;

    default:
      break;
  }
}

// Assignment -----------------------------------------------------------------

// We return a null value from this rather than the result of doing the append.
// See ValuePlusEquals for rationale.
Value ExecuteEquals(Scope* scope,
                    const BinaryOpNode* op_node,
                    const Token& left,
                    const Value& right,
                    Err* err) {
  const Value* old_value = scope->GetValue(left.value(), false);
  if (old_value) {
    // Throw an error when overwriting a nonempty list with another nonempty
    // list item. This is to detect the case where you write
    //   defines = ["FOO"]
    // and you overwrote inherited ones, when instead you mean to append:
    //   defines += ["FOO"]
    if (old_value->type() == Value::LIST &&
        !old_value->list_value().empty() &&
        right.type() == Value::LIST &&
        !right.list_value().empty()) {
      *err = Err(op_node->left()->GetRange(), "Replacing nonempty list.",
          std::string("This overwrites a previously-defined nonempty list ") +
          "(length " +
          base::IntToString(static_cast<int>(old_value->list_value().size()))
          + ").");
      err->AppendSubErr(Err(*old_value, "for previous definition",
          "with another one (length " +
          base::IntToString(static_cast<int>(right.list_value().size())) +
          "). Did you mean " +
          "\"+=\" to append instead? If you\nreally want to do this, do\n  " +
          left.value().as_string() + " = []\nbefore reassigning."));
      return Value();
    }
  }
  if (err->has_error())
    return Value();

  if (right.type() == Value::LIST && left.value() == kSourcesName) {
    // Assigning to sources, filter the list. Here we do the filtering and
    // copying in one step to save an extra list copy (the lists may be
    // long).
    Value* set_value = scope->SetValue(left.value(),
                                       Value(op_node, Value::LIST), op_node);
    set_value->list_value().reserve(right.list_value().size());
    AppendFilteredSourcesToValue(scope, right, set_value);
  } else {
    // Normal value set, just copy it.
    scope->SetValue(left.value(), right, op_node->right());
  }
  return Value();
}

// allow_type_conversion indicates if we're allowed to change the type of the
// left value. This is set to true when doing +, and false when doing +=.
//
// Note that we return Value() from here, which is different than C++. This
// means you can't do clever things like foo = [ bar += baz ] to simultaneously
// append to and use a value. This is basically never needed in out build
// scripts and is just as likely an error as the intended behavior, and it also
// involves a copy of the value when it's returned. Many times we're appending
// to large lists, and copying the value to discard it for the next statement
// is very wasteful.
void ValuePlusEquals(const Scope* scope,
                     const BinaryOpNode* op_node,
                     const Token& left_token,
                     Value* left,
                     const Value& right,
                     bool allow_type_conversion,
                     Err* err) {
  switch (left->type()) {
    // Left-hand-side int.
    case Value::INTEGER:
      switch (right.type()) {
        case Value::INTEGER:  // int + int -> addition.
          left->int_value() += right.int_value();
          return;

        case Value::STRING:  // int + string -> string concat.
          if (allow_type_conversion) {
            *left = Value(op_node,
                base::Int64ToString(left->int_value()) + right.string_value());
            return;
          }
          break;

        default:
          break;
      }
      break;

    // Left-hand-side string.
    case Value::STRING:
      switch (right.type()) {
        case Value::INTEGER:  // string + int -> string concat.
          left->string_value().append(base::Int64ToString(right.int_value()));
          return;

        case Value::STRING:  // string + string -> string contat.
          left->string_value().append(right.string_value());
          return;

        default:
          break;
      }
      break;

    // Left-hand-side list.
    case Value::LIST:
      switch (right.type()) {
        case Value::LIST:  // list + list -> list concat.
          if (left_token.value() == kSourcesName) {
            // Filter additions through the assignment filter.
            AppendFilteredSourcesToValue(scope, right, left);
          } else {
            // Normal list concat.
            for (const auto& value : right.list_value())
              left->list_value().push_back(value);
          }
          return;

        default:
          *err = Err(op_node->op(), "Incompatible types to add.",
              "To append a single item to a list do \"foo += [ bar ]\".");
          return;
      }

    default:
      break;
  }

  *err = Err(op_node->op(), "Incompatible types to add.",
      std::string("I see a ") + Value::DescribeType(left->type()) + " and a " +
      Value::DescribeType(right.type()) + ".");
}

Value ExecutePlusEquals(Scope* scope,
                        const BinaryOpNode* op_node,
                        const Token& left,
                        const Value& right,
                        Err* err) {
  // We modify in-place rather than doing read-modify-write to avoid
  // copying large lists.
  Value* left_value =
      scope->GetValueForcedToCurrentScope(left.value(), op_node);
  if (!left_value) {
    *err = Err(left, "Undefined variable for +=.",
        "I don't have something with this name in scope now.");
    return Value();
  }
  ValuePlusEquals(scope, op_node, left, left_value, right, false, err);
  left_value->set_origin(op_node);
  scope->MarkUnused(left.value());
  return Value();
}

// We return a null value from this rather than the result of doing the append.
// See ValuePlusEquals for rationale.
void ValueMinusEquals(const BinaryOpNode* op_node,
                      Value* left,
                      const Value& right,
                      bool allow_type_conversion,
                      Err* err) {
  switch (left->type()) {
    // Left-hand-side int.
    case Value::INTEGER:
      switch (right.type()) {
        case Value::INTEGER:  // int - int -> subtraction.
          left->int_value() -= right.int_value();
          return;

        default:
          break;
      }
      break;

    // Left-hand-side string.
    case Value::STRING:
      break;  // All are errors.

    // Left-hand-side list.
    case Value::LIST:
      if (right.type() != Value::LIST) {
        *err = Err(op_node->op(), "Incompatible types to subtract.",
            "To remove a single item from a list do \"foo -= [ bar ]\".");
      } else {
        RemoveMatchesFromList(op_node, left, right, err);
      }
      return;

    default:
      break;
  }

  *err = Err(op_node->op(), "Incompatible types to subtract.",
      std::string("I see a ") + Value::DescribeType(left->type()) + " and a " +
      Value::DescribeType(right.type()) + ".");
}

Value ExecuteMinusEquals(Scope* scope,
                         const BinaryOpNode* op_node,
                         const Token& left,
                         const Value& right,
                         Err* err) {
  Value* left_value =
      scope->GetValueForcedToCurrentScope(left.value(), op_node);
  if (!left_value) {
    *err = Err(left, "Undefined variable for -=.",
        "I don't have something with this name in scope now.");
    return Value();
  }
  ValueMinusEquals(op_node, left_value, right, false, err);
  left_value->set_origin(op_node);
  scope->MarkUnused(left.value());
  return Value();
}

// Plus/Minus -----------------------------------------------------------------

Value ExecutePlus(Scope* scope,
                  const BinaryOpNode* op_node,
                  const Value& left,
                  const Value& right,
                  Err* err) {
  Value ret = left;
  ValuePlusEquals(scope, op_node, Token(), &ret, right, true, err);
  ret.set_origin(op_node);
  return ret;
}

Value ExecuteMinus(Scope* scope,
                   const BinaryOpNode* op_node,
                   const Value& left,
                   const Value& right,
                   Err* err) {
  Value ret = left;
  ValueMinusEquals(op_node, &ret, right, true, err);
  ret.set_origin(op_node);
  return ret;
}

// Comparison -----------------------------------------------------------------

Value ExecuteEqualsEquals(Scope* scope,
                          const BinaryOpNode* op_node,
                          const Value& left,
                          const Value& right,
                          Err* err) {
  if (left == right)
    return Value(op_node, true);
  return Value(op_node, false);
}

Value ExecuteNotEquals(Scope* scope,
                       const BinaryOpNode* op_node,
                       const Value& left,
                       const Value& right,
                       Err* err) {
  // Evaluate in terms of ==.
  Value result = ExecuteEqualsEquals(scope, op_node, left, right, err);
  result.boolean_value() = !result.boolean_value();
  return result;
}

Value FillNeedsTwoIntegersError(const BinaryOpNode* op_node,
                                const Value& left,
                                const Value& right,
                                Err* err) {
  *err = Err(op_node, "Comparison requires two integers.",
             "This operator can only compare two integers.");
  err->AppendRange(left.origin()->GetRange());
  err->AppendRange(right.origin()->GetRange());
  return Value();
}

Value ExecuteLessEquals(Scope* scope,
                        const BinaryOpNode* op_node,
                        const Value& left,
                        const Value& right,
                        Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() <= right.int_value());
}

Value ExecuteGreaterEquals(Scope* scope,
                           const BinaryOpNode* op_node,
                           const Value& left,
                           const Value& right,
                           Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() >= right.int_value());
}

Value ExecuteGreater(Scope* scope,
                     const BinaryOpNode* op_node,
                     const Value& left,
                     const Value& right,
                     Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() > right.int_value());
}

Value ExecuteLess(Scope* scope,
                  const BinaryOpNode* op_node,
                  const Value& left,
                  const Value& right,
                  Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() < right.int_value());
}

// Binary ----------------------------------------------------------------------

Value ExecuteOr(Scope* scope,
                const BinaryOpNode* op_node,
                const ParseNode* left_node,
                const ParseNode* right_node,
                Err* err) {
  Value left = GetValueOrFillError(op_node, left_node, "left", scope, err);
  if (err->has_error())
    return Value();
  if (left.type() != Value::BOOLEAN) {
    *err = Err(op_node->left(), "Left side of || operator is not a boolean.",
        "Type is \"" + std::string(Value::DescribeType(left.type())) +
        "\" instead.");
    return Value();
  }
  if (left.boolean_value())
    return Value(op_node, left.boolean_value());

  Value right = GetValueOrFillError(op_node, right_node, "right", scope, err);
  if (err->has_error())
    return Value();
  if (right.type() != Value::BOOLEAN) {
    *err = Err(op_node->right(), "Right side of || operator is not a boolean.",
        "Type is \"" + std::string(Value::DescribeType(right.type())) +
        "\" instead.");
    return Value();
  }

  return Value(op_node, left.boolean_value() || right.boolean_value());
}

Value ExecuteAnd(Scope* scope,
                 const BinaryOpNode* op_node,
                 const ParseNode* left_node,
                 const ParseNode* right_node,
                 Err* err) {
  Value left = GetValueOrFillError(op_node, left_node, "left", scope, err);
  if (err->has_error())
    return Value();
  if (left.type() != Value::BOOLEAN) {
    *err = Err(op_node->left(), "Left side of && operator is not a boolean.",
        "Type is \"" + std::string(Value::DescribeType(left.type())) +
        "\" instead.");
    return Value();
  }
  if (!left.boolean_value())
    return Value(op_node, left.boolean_value());

  Value right = GetValueOrFillError(op_node, right_node, "right", scope, err);
  if (err->has_error())
    return Value();
  if (right.type() != Value::BOOLEAN) {
    *err = Err(op_node->right(), "Right side of && operator is not a boolean.",
        "Type is \"" + std::string(Value::DescribeType(right.type())) +
        "\" instead.");
    return Value();
  }
  return Value(op_node, left.boolean_value() && right.boolean_value());
}

}  // namespace

// ----------------------------------------------------------------------------

Value ExecuteUnaryOperator(Scope* scope,
                           const UnaryOpNode* op_node,
                           const Value& expr,
                           Err* err) {
  DCHECK(op_node->op().type() == Token::BANG);

  if (expr.type() != Value::BOOLEAN) {
    *err = Err(op_node, "Operand of ! operator is not a boolean.",
        "Type is \"" + std::string(Value::DescribeType(expr.type())) +
        "\" instead.");
    return Value();
  }
  // TODO(scottmg): Why no unary minus?
  return Value(op_node, !expr.boolean_value());
}

Value ExecuteBinaryOperator(Scope* scope,
                            const BinaryOpNode* op_node,
                            const ParseNode* left,
                            const ParseNode* right,
                            Err* err) {
  const Token& op = op_node->op();

  // First handle the ones that take an lvalue.
  if (op.type() == Token::EQUAL ||
      op.type() == Token::PLUS_EQUALS ||
      op.type() == Token::MINUS_EQUALS) {
    const IdentifierNode* left_id = left->AsIdentifier();
    if (!left_id) {
      *err = Err(op, "Operator requires a lvalue.",
                 "This thing on the left is not an identifier.");
      err->AppendRange(left->GetRange());
      return Value();
    }
    const Token& dest = left_id->value();

    Value right_value = right->Execute(scope, err);
    if (err->has_error())
      return Value();
    if (right_value.type() == Value::NONE) {
      *err = Err(op, "Operator requires a rvalue.",
                 "This thing on the right does not evaluate to a value.");
      err->AppendRange(right->GetRange());
      return Value();
    }

    if (op.type() == Token::EQUAL)
      return ExecuteEquals(scope, op_node, dest, right_value, err);
    if (op.type() == Token::PLUS_EQUALS)
      return ExecutePlusEquals(scope, op_node, dest, right_value, err);
    if (op.type() == Token::MINUS_EQUALS)
      return ExecuteMinusEquals(scope, op_node, dest, right_value, err);
    NOTREACHED();
    return Value();
  }

  // ||, &&. Passed the node instead of the value so that they can avoid
  // evaluating the RHS on early-out.
  if (op.type() == Token::BOOLEAN_OR)
    return ExecuteOr(scope, op_node, left, right, err);
  if (op.type() == Token::BOOLEAN_AND)
    return ExecuteAnd(scope, op_node, left, right, err);

  Value left_value = GetValueOrFillError(op_node, left, "left", scope, err);
  if (err->has_error())
    return Value();
  Value right_value = GetValueOrFillError(op_node, right, "right", scope, err);
  if (err->has_error())
    return Value();

  // +, -.
  if (op.type() == Token::MINUS)
    return ExecuteMinus(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::PLUS)
    return ExecutePlus(scope, op_node, left_value, right_value, err);

  // Comparisons.
  if (op.type() == Token::EQUAL_EQUAL)
    return ExecuteEqualsEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::NOT_EQUAL)
    return ExecuteNotEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::GREATER_EQUAL)
    return ExecuteGreaterEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::LESS_EQUAL)
    return ExecuteLessEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::GREATER_THAN)
    return ExecuteGreater(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::LESS_THAN)
    return ExecuteLess(scope, op_node, left_value, right_value, err);

  return Value();
}
