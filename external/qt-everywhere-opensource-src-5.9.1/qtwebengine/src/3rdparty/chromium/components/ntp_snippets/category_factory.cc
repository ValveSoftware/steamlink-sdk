// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_factory.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace ntp_snippets {

CategoryFactory::CategoryFactory() {
  // Add all local categories in a fixed order.
  AddKnownCategory(KnownCategories::DOWNLOADS);
  AddKnownCategory(KnownCategories::RECENT_TABS);
  AddKnownCategory(KnownCategories::FOREIGN_TABS);
  AddKnownCategory(KnownCategories::BOOKMARKS);
  AddKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES);

  DCHECK_EQ(static_cast<size_t>(KnownCategories::LOCAL_CATEGORIES_COUNT),
            ordered_categories_.size());

  // Known remote categories come after. Other remote categories will be ordered
  // after these depending on when providers notify us about a related change.
  AddKnownCategory(KnownCategories::ARTICLES);
}

CategoryFactory::~CategoryFactory() = default;

Category CategoryFactory::FromKnownCategory(KnownCategories known_category) {
  if (known_category < KnownCategories::LOCAL_CATEGORIES_COUNT) {
    // Local categories should have been added already.
    DCHECK(CategoryExists(static_cast<int>(known_category)));
  } else {
    DCHECK_GT(known_category, KnownCategories::REMOTE_CATEGORIES_OFFSET);
  }
  return InternalFromID(static_cast<int>(known_category));
}

Category CategoryFactory::FromRemoteCategory(int remote_category) {
  DCHECK_GT(remote_category, 0);
  return InternalFromID(
      static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET) +
      remote_category);
}

Category CategoryFactory::FromIDValue(int id) {
  DCHECK_GE(id, 0);
  DCHECK(id < static_cast<int>(KnownCategories::LOCAL_CATEGORIES_COUNT) ||
         id > static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET));
  return InternalFromID(id);
}

bool CategoryFactory::CompareCategories(const Category& left,
                                        const Category& right) const {
  if (left == right)
    return false;
  return std::find(ordered_categories_.begin(), ordered_categories_.end(),
                   left) < std::find(ordered_categories_.begin(),
                                     ordered_categories_.end(), right);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

bool CategoryFactory::CategoryExists(int id) {
  return std::find(ordered_categories_.begin(), ordered_categories_.end(),
                   Category(id)) != ordered_categories_.end();
}

void CategoryFactory::AddKnownCategory(KnownCategories known_category) {
  InternalFromID(static_cast<int>(known_category));
}

Category CategoryFactory::InternalFromID(int id) {
  auto it = std::find(ordered_categories_.begin(), ordered_categories_.end(),
                      Category(id));
  if (it != ordered_categories_.end())
    return *it;

  Category category(id);
  ordered_categories_.push_back(category);
  return category;
}

}  // namespace ntp_snippets
