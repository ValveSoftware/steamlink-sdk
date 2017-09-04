// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace ntp_snippets {

// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class ContentSuggestionsCardLayout {
  // Uses all fields.
  FULL_CARD,

  // No snippet_text and no thumbnail image.
  MINIMAL_CARD
};

// Contains static meta information about a Category.
class CategoryInfo {
 public:
  CategoryInfo(const base::string16& title,
               ContentSuggestionsCardLayout card_layout,
               bool has_more_action,
               bool has_reload_action,
               bool has_view_all_action,
               bool show_if_empty,
               const base::string16& no_suggestions_message);
  CategoryInfo() = delete;
  CategoryInfo(CategoryInfo&&);
  CategoryInfo(const CategoryInfo&);
  CategoryInfo& operator=(CategoryInfo&&);
  CategoryInfo& operator=(const CategoryInfo&);
  ~CategoryInfo();

  // Localized title of the category.
  const base::string16& title() const { return title_; }

  // Layout of the cards to be used to display suggestions in this category.
  ContentSuggestionsCardLayout card_layout() const { return card_layout_; }

  // Whether the category supports a "More" action, that triggers fetching more
  // suggestions for the category, while keeping the current ones.
  bool has_more_action() const { return has_more_action_; }

  // Whether the category supports a "Reload" action, that triggers fetching new
  // suggestions to replace the current ones.
  bool has_reload_action() const { return has_reload_action_; }

  // Whether the category supports a "ViewAll" action, that triggers displaying
  // all the content related to the current categories.
  bool has_view_all_action() const { return has_view_all_action_; }

  // Whether this category should be shown if it offers no suggestions.
  bool show_if_empty() const { return show_if_empty_; }

  // The message to show if there are no suggestions in this category. Note that
  // this matters even if |show_if_empty()| is false: The message still shows
  // up when the user dismisses all suggestions in the category.
  const base::string16& no_suggestions_message() const {
    return no_suggestions_message_;
  }

 private:
  base::string16 title_;
  ContentSuggestionsCardLayout card_layout_;

  // Supported actions for the category.
  bool has_more_action_;
  bool has_reload_action_;
  bool has_view_all_action_;

  // Whether to show the category if a fetch returns no suggestions.
  bool show_if_empty_;
  base::string16 no_suggestions_message_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
