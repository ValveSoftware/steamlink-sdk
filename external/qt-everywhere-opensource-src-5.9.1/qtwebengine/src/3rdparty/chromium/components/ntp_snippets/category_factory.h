// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/ntp_snippets/category.h"

namespace ntp_snippets {

// Creates and orders Category instances.
class CategoryFactory {
 public:
  CategoryFactory();
  ~CategoryFactory();

  // Creates a category from a KnownCategory value. The passed |known_category|
  // must not be one of the special values (LOCAL_CATEGORIES_COUNT or
  // REMOTE_CATEGORIES_OFFSET).
  Category FromKnownCategory(KnownCategories known_category);

  // Creates a category from a category identifier delivered by the server.
  // |remote_category| must be positive.
  // Note that remote categories are ordered in the order in which they were
  // first created by calling this method.
  Category FromRemoteCategory(int remote_category);

  // Creates a category from an ID as returned by |Category::id()|.
  // |id| must be a non-negative value.
  Category FromIDValue(int id);

  // Compares the given categories according to a strict ordering, returning
  // true if and only if |left| is strictly less than |right|.
  // This method satisfies the "Compare" contract required by sort algorithms.
  // The order is determined as follows: All local categories go first, in a
  // specific order hard-coded in the |CategoryFactory| constructor. All remote
  // categories follow in the order in which they were first created through
  // |FromRemoteCategory|.
  bool CompareCategories(const Category& left, const Category& right) const;

 private:
  bool CategoryExists(int id);
  void AddKnownCategory(KnownCategories known_category);
  Category InternalFromID(int id);

  // Stores all known categories in the order which is also returned by
  // |CompareCategories|.
  std::vector<Category> ordered_categories_;

  DISALLOW_COPY_AND_ASSIGN(CategoryFactory);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_
