// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_H_

#include <ostream>

namespace ntp_snippets {

class CategoryFactory;

// These are the categories that the client knows about.
// The values before LOCAL_CATEGORIES_COUNT are the categories that are provided
// locally on the device. Categories provided by the server (IDs strictly larger
// than REMOTE_CATEGORIES_OFFSET) only need to be hard-coded here if they need
// to be recognized by the client implementation.
// NOTE: These are persisted, so don't reorder or remove values, and insert new
// values only in the appropriate places marked below.
// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class KnownCategories {
  // Pages recently downloaded during normal navigation.
  RECENT_TABS,

  // Pages downloaded by the user for offline consumption.
  DOWNLOADS,

  // Recently used bookmarks.
  BOOKMARKS,

  // Physical Web page available in the vicinity.
  PHYSICAL_WEB_PAGES,

  // Pages recently browsed to on other devices.
  FOREIGN_TABS,
  // INSERT NEW LOCAL CATEGORIES HERE!

  // Follows the last local category.
  LOCAL_CATEGORIES_COUNT,

  // Remote categories come after this.
  REMOTE_CATEGORIES_OFFSET = 10000,

  // Articles for you.
  ARTICLES,
  // INSERT NEW REMOTE CATEGORIES HERE!

  // Tracks the last known remote category
  LAST_KNOWN_REMOTE_CATEGORY = ARTICLES,
};

// A category groups ContentSuggestions which belong together. Use the
// CategoryFactory to obtain instances.
class Category {
 public:
  // An arbitrary but consistent ordering. Can be used to look up categories in
  // a std::map, but should not be used to order categories for other purposes.
  struct CompareByID;

  // Returns a non-negative identifier that is unique for the category and can
  // be converted back to a Category instance using
  // |CategoryFactory::FromIDValue(id)|.
  int id() const { return id_; }

  // Returns whether this category matches the given |known_category|.
  bool IsKnownCategory(KnownCategories known_category) const;

 private:
  friend class CategoryFactory;

  explicit Category(int id);

  int id_;

  // Allow copy and assignment.
};

bool operator==(const Category& left, const Category& right);

bool operator!=(const Category& left, const Category& right);

struct Category::CompareByID {
  bool operator()(const Category& left, const Category& right) const;
};

std::ostream& operator<<(std::ostream& os, const Category& obj);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_H_
