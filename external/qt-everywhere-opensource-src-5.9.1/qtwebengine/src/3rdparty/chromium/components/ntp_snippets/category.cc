// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category.h"

#include "base/logging.h"

namespace ntp_snippets {

Category::Category(int id) : id_(id) {}

bool Category::IsKnownCategory(KnownCategories known_category) const {
  DCHECK_NE(known_category, KnownCategories::LOCAL_CATEGORIES_COUNT);
  DCHECK_NE(known_category, KnownCategories::REMOTE_CATEGORIES_OFFSET);
  return id_ == static_cast<int>(known_category);
}

bool operator==(const Category& left, const Category& right) {
  return left.id() == right.id();
}

bool operator!=(const Category& left, const Category& right) {
  return !(left == right);
}

bool Category::CompareByID::operator()(const Category& left,
                                       const Category& right) const {
  return left.id() < right.id();
}

std::ostream& operator<<(std::ostream& os, const Category& obj) {
  os << obj.id();
  return os;
}

}  // namespace ntp_snippets
