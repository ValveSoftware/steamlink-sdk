// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a builders for DictionaryValue and ListValue.  These
// aren't specific to extensions and could move up to base/ if there's interest
// from other sub-projects.
//
// The pattern is to write:
//
//  std::unique_ptr<BuiltType> result(FooBuilder()
//                               .Set(args)
//                               .Set(args)
//                               .Build());
//
// The Build() method invalidates its builder, and returns ownership of the
// built value.
//
// These objects are intended to be used as temporaries rather than stored
// anywhere, so the use of non-const reference parameters is likely to cause
// less confusion than usual.

#ifndef EXTENSIONS_COMMON_VALUE_BUILDER_H_
#define EXTENSIONS_COMMON_VALUE_BUILDER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace extensions {

class ListBuilder;

class DictionaryBuilder {
 public:
  DictionaryBuilder();
  explicit DictionaryBuilder(const base::DictionaryValue& init);
  ~DictionaryBuilder();

  // Can only be called once, after which it's invalid to use the builder.
  std::unique_ptr<base::DictionaryValue> Build() { return std::move(dict_); }

  // Immediately serializes the current state to JSON. Can be called as many
  // times as you like.
  std::string ToJSON() const;

  DictionaryBuilder& Set(const std::string& path, int in_value);
  DictionaryBuilder& Set(const std::string& path, double in_value);
  DictionaryBuilder& Set(const std::string& path, const std::string& in_value);
  DictionaryBuilder& Set(const std::string& path,
                         const base::string16& in_value);
  DictionaryBuilder& Set(const std::string& path,
                         std::unique_ptr<base::Value> in_value);

  // Named differently because overload resolution is too eager to
  // convert implicitly to bool.
  DictionaryBuilder& SetBoolean(const std::string& path, bool in_value);

 private:
  std::unique_ptr<base::DictionaryValue> dict_;
};

class ListBuilder {
 public:
  ListBuilder();
  explicit ListBuilder(const base::ListValue& init);
  ~ListBuilder();

  // Can only be called once, after which it's invalid to use the builder.
  std::unique_ptr<base::ListValue> Build() { return std::move(list_); }

  ListBuilder& Append(int in_value);
  ListBuilder& Append(double in_value);
  ListBuilder& Append(const std::string& in_value);
  ListBuilder& Append(const base::string16& in_value);
  ListBuilder& Append(std::unique_ptr<base::Value> in_value);

  // Named differently because overload resolution is too eager to
  // convert implicitly to bool.
  ListBuilder& AppendBoolean(bool in_value);

 private:
  std::unique_ptr<base::ListValue> list_;

  DISALLOW_COPY_AND_ASSIGN(ListBuilder);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VALUE_BUILDER_H_
