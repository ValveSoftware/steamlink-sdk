// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"

#include <stdint.h>

#include "base/i18n/case_conversion.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.h"

namespace content {

// Given a node, populate a vector with all of the strings from that node's
// attributes that might be relevant for a text search.
void GetNodeStrings(BrowserAccessibility* node,
                    std::vector<base::string16>* strings) {
  if (node->HasStringAttribute(ui::AX_ATTR_NAME))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_NAME));
  if (node->HasStringAttribute(ui::AX_ATTR_DESCRIPTION))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_DESCRIPTION));
  if (node->HasStringAttribute(ui::AX_ATTR_VALUE))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_VALUE));
  if (node->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_PLACEHOLDER));
}

OneShotAccessibilityTreeSearch::OneShotAccessibilityTreeSearch(
    BrowserAccessibility* scope)
    : tree_(scope->manager()),
      scope_node_(scope),
      start_node_(scope),
      direction_(OneShotAccessibilityTreeSearch::FORWARDS),
      result_limit_(UNLIMITED_RESULTS),
      immediate_descendants_only_(false),
      visible_only_(false),
      did_search_(false) {
}

OneShotAccessibilityTreeSearch::~OneShotAccessibilityTreeSearch() {
}

void OneShotAccessibilityTreeSearch::SetStartNode(
    BrowserAccessibility* start_node) {
  DCHECK(!did_search_);
  CHECK(start_node);

  if (!scope_node_->GetParent() ||
      start_node->IsDescendantOf(scope_node_->GetParent())) {
    start_node_ = start_node;
  }
}

void OneShotAccessibilityTreeSearch::SetDirection(Direction direction) {
  DCHECK(!did_search_);
  direction_ = direction;
}

void OneShotAccessibilityTreeSearch::SetResultLimit(int result_limit) {
  DCHECK(!did_search_);
  result_limit_ = result_limit;
}

void OneShotAccessibilityTreeSearch::SetImmediateDescendantsOnly(
    bool immediate_descendants_only) {
  DCHECK(!did_search_);
  immediate_descendants_only_ = immediate_descendants_only;
}

void OneShotAccessibilityTreeSearch::SetVisibleOnly(bool visible_only) {
  DCHECK(!did_search_);
  visible_only_ = visible_only;
}

void OneShotAccessibilityTreeSearch::SetSearchText(const std::string& text) {
  DCHECK(!did_search_);
  search_text_ = text;
}

void OneShotAccessibilityTreeSearch::AddPredicate(
    AccessibilityMatchPredicate predicate) {
  DCHECK(!did_search_);
  predicates_.push_back(predicate);
}

size_t OneShotAccessibilityTreeSearch::CountMatches() {
  if (!did_search_)
    Search();

  return matches_.size();
}

BrowserAccessibility* OneShotAccessibilityTreeSearch::GetMatchAtIndex(
    size_t index) {
  if (!did_search_)
    Search();

  CHECK(index < matches_.size());
  return matches_[index];
}

void OneShotAccessibilityTreeSearch::Search() {
  if (immediate_descendants_only_) {
    SearchByIteratingOverChildren();
  } else {
    SearchByWalkingTree();
  }
  did_search_ = true;
}

void OneShotAccessibilityTreeSearch::SearchByIteratingOverChildren() {
  // Iterate over the children of scope_node_.
  // If start_node_ is specified, iterate over the first child past that
  // node.

  uint32_t count = scope_node_->PlatformChildCount();
  if (count == 0)
    return;

  // We only care about immediate children of scope_node_, so walk up
  // start_node_ until we get to an immediate child. If it isn't a child,
  // we ignore start_node_.
  while (start_node_ && start_node_->GetParent() != scope_node_)
    start_node_ = start_node_->GetParent();

  uint32_t index = (direction_ == FORWARDS ? 0 : count - 1);
  if (start_node_) {
    index = start_node_->GetIndexInParent();
    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }

  while (index < count &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    BrowserAccessibility* node = scope_node_->PlatformGetChild(index);
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }
}

void OneShotAccessibilityTreeSearch::SearchByWalkingTree() {
  BrowserAccessibility* node = nullptr;
  node = start_node_;
  if (node != scope_node_) {
    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(start_node_);
    else
      node = tree_->PreviousInTreeOrder(start_node_);
  }

  BrowserAccessibility* stop_node = scope_node_->GetParent();
  while (node &&
         node != stop_node &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(node);
    else
      node = tree_->PreviousInTreeOrder(node);
  }
}

bool OneShotAccessibilityTreeSearch::Matches(BrowserAccessibility* node) {
  for (size_t i = 0; i < predicates_.size(); ++i) {
    if (!predicates_[i](start_node_, node))
      return false;
  }

  if (visible_only_) {
    if (node->HasState(ui::AX_STATE_INVISIBLE) ||
        node->HasState(ui::AX_STATE_OFFSCREEN)) {
      return false;
    }
  }

  if (!search_text_.empty()) {
    base::string16 search_text_lower =
      base::i18n::ToLower(base::UTF8ToUTF16(search_text_));
    std::vector<base::string16> node_strings;
    GetNodeStrings(node, &node_strings);
    bool found_text_match = false;
    for (size_t i = 0; i < node_strings.size(); ++i) {
      base::string16 node_string_lower =
        base::i18n::ToLower(node_strings[i]);
      if (node_string_lower.find(search_text_lower) !=
          base::string16::npos) {
        found_text_match = true;
        break;
      }
    }
    if (!found_text_match)
      return false;
  }

  return true;
}

//
// Predicates
//

bool AccessibilityArticlePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return node->GetRole() == ui::AX_ROLE_ARTICLE;
}

bool AccessibilityButtonPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ui::AX_ROLE_BUTTON:
    case ui::AX_ROLE_MENU_BUTTON:
    case ui::AX_ROLE_POP_UP_BUTTON:
    case ui::AX_ROLE_SWITCH:
    case ui::AX_ROLE_TOGGLE_BUTTON:
      return true;
    default:
      return false;
  }
}

bool AccessibilityBlockquotePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return node->GetRole() == ui::AX_ROLE_BLOCKQUOTE;
}

bool AccessibilityCheckboxPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_CHECK_BOX ||
          node->GetRole() == ui::AX_ROLE_MENU_ITEM_CHECK_BOX);
}

bool AccessibilityComboboxPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_COMBO_BOX ||
          node->GetRole() == ui::AX_ROLE_POP_UP_BUTTON);
}

bool AccessibilityControlPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  if (node->IsControl())
    return true;
  if (node->HasState(ui::AX_STATE_FOCUSABLE) &&
      node->GetRole() != ui::AX_ROLE_IFRAME &&
      node->GetRole() != ui::AX_ROLE_IFRAME_PRESENTATIONAL &&
      node->GetRole() != ui::AX_ROLE_IMAGE_MAP_LINK &&
      node->GetRole() != ui::AX_ROLE_LINK &&
      node->GetRole() != ui::AX_ROLE_WEB_AREA &&
      node->GetRole() != ui::AX_ROLE_ROOT_WEB_AREA) {
    return true;
  }
  return false;
}

bool AccessibilityFocusablePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  bool focusable = node->HasState(ui::AX_STATE_FOCUSABLE);
  if (node->GetRole() == ui::AX_ROLE_IFRAME ||
      node->GetRole() == ui::AX_ROLE_IFRAME_PRESENTATIONAL ||
      node->GetRole() == ui::AX_ROLE_WEB_AREA ||
      node->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA) {
    focusable = false;
  }
  return focusable;
}

bool AccessibilityGraphicPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return node->GetRole() == ui::AX_ROLE_IMAGE;
}

bool AccessibilityHeadingPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING);
}

bool AccessibilityH1Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 1);
}

bool AccessibilityH2Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 2);
}

bool AccessibilityH3Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 3);
}

bool AccessibilityH4Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 4);
}

bool AccessibilityH5Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 5);
}

bool AccessibilityH6Predicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 6);
}

bool AccessibilityHeadingSameLevelPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_HEADING &&
          start->GetRole() == ui::AX_ROLE_HEADING &&
          (node->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) ==
           start->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL)));
}

bool AccessibilityFramePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  if (node->IsWebAreaForPresentationalIframe())
    return false;
  if (!node->GetParent())
    return false;
  return (node->GetRole() == ui::AX_ROLE_WEB_AREA ||
          node->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA);
}

bool AccessibilityLandmarkPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ui::AX_ROLE_APPLICATION:
    case ui::AX_ROLE_ARTICLE:
    case ui::AX_ROLE_BANNER:
    case ui::AX_ROLE_COMPLEMENTARY:
    case ui::AX_ROLE_CONTENT_INFO:
    case ui::AX_ROLE_MAIN:
    case ui::AX_ROLE_NAVIGATION:
    case ui::AX_ROLE_SEARCH:
    case ui::AX_ROLE_REGION:
      return true;
    default:
      return false;
  }
}

bool AccessibilityLinkPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_LINK ||
          node->GetRole() == ui::AX_ROLE_IMAGE_MAP_LINK);
}

bool AccessibilityListPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_LIST_BOX ||
          node->GetRole() == ui::AX_ROLE_LIST ||
          node->GetRole() == ui::AX_ROLE_DESCRIPTION_LIST);
}

bool AccessibilityListItemPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_LIST_ITEM ||
          node->GetRole() == ui::AX_ROLE_DESCRIPTION_LIST_TERM ||
          node->GetRole() == ui::AX_ROLE_LIST_BOX_OPTION);
}

bool AccessibilityLiveRegionPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return node->HasStringAttribute(ui::AX_ATTR_LIVE_STATUS);
}

bool AccessibilityMainPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_MAIN);
}

bool AccessibilityMediaPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  const std::string& tag = node->GetStringAttribute(ui::AX_ATTR_HTML_TAG);
  return tag == "audio" || tag == "video";
}

bool AccessibilityRadioButtonPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_RADIO_BUTTON ||
          node->GetRole() == ui::AX_ROLE_MENU_ITEM_RADIO);
}

bool AccessibilityRadioGroupPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return node->GetRole() == ui::AX_ROLE_RADIO_GROUP;
}

bool AccessibilityTablePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->GetRole() == ui::AX_ROLE_TABLE ||
          node->GetRole() == ui::AX_ROLE_GRID);
}

bool AccessibilityTextfieldPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->IsSimpleTextControl() || node->IsRichTextControl());
}

bool AccessibilityTextStyleBoldPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  int32_t style = node->GetIntAttribute(ui::AX_ATTR_TEXT_STYLE);
  return 0 != (style & ui::AX_TEXT_STYLE_BOLD);
}

bool AccessibilityTextStyleItalicPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  int32_t style = node->GetIntAttribute(ui::AX_ATTR_TEXT_STYLE);
  return 0 != (style & ui::AX_TEXT_STYLE_BOLD);
}

bool AccessibilityTextStyleUnderlinePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  int32_t style = node->GetIntAttribute(ui::AX_ATTR_TEXT_STYLE);
  return 0 != (style & ui::AX_TEXT_STYLE_UNDERLINE);
}

bool AccessibilityTreePredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return (node->IsSimpleTextControl() || node->IsRichTextControl());
}

bool AccessibilityUnvisitedLinkPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return ((node->GetRole() == ui::AX_ROLE_LINK ||
           node->GetRole() == ui::AX_ROLE_IMAGE_MAP_LINK) &&
          !node->HasState(ui::AX_STATE_VISITED));
}

bool AccessibilityVisitedLinkPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  return ((node->GetRole() == ui::AX_ROLE_LINK ||
           node->GetRole() == ui::AX_ROLE_IMAGE_MAP_LINK) &&
          node->HasState(ui::AX_STATE_VISITED));
}

}  // namespace content
