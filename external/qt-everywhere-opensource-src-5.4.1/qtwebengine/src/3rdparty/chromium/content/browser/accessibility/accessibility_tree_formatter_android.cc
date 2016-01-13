// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_android.h"

using base::StringPrintf;

namespace content {

namespace {
const char* BOOL_ATTRIBUTES[] = {
  "checkable",
  "checked",
  "clickable",
  "collection",
  "collection_item",
  "content_invalid",
  "disabled",
  "dismissable",
  "editable_text",
  "focusable",
  "focused",
  "heading",
  "hierarchical",
  "invisible",
  "multiline",
  "password",
  "range",
  "scrollable",
  "selected"
};

const char* STRING_ATTRIBUTES[] = {
  "name"
};

const char* INT_ATTRIBUTES[] = {
  "item_index",
  "item_count",
  "row_count",
  "column_count",
  "row_index",
  "row_span",
  "column_index",
  "column_span",
  "input_type",
  "live_region_type",
  "range_min",
  "range_max",
  "range_current_value",
};
}

void AccessibilityTreeFormatter::Initialize() {
}

void AccessibilityTreeFormatter::AddProperties(
    const BrowserAccessibility& node, base::DictionaryValue* dict) {
  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // Class name.
  dict->SetString("class", android_node->GetClassName());

  // Bool attributes.
  dict->SetBoolean("checkable", android_node->IsCheckable());
  dict->SetBoolean("checked", android_node->IsChecked());
  dict->SetBoolean("clickable", android_node->IsClickable());
  dict->SetBoolean("collection", android_node->IsCollection());
  dict->SetBoolean("collection_item", android_node->IsCollectionItem());
  dict->SetBoolean("disabled", !android_node->IsEnabled());
  dict->SetBoolean("dismissable", android_node->IsDismissable());
  dict->SetBoolean("editable_text", android_node->IsEditableText());
  dict->SetBoolean("focusable", android_node->IsFocusable());
  dict->SetBoolean("focused", android_node->IsFocused());
  dict->SetBoolean("heading", android_node->IsHeading());
  dict->SetBoolean("hierarchical", android_node->IsHierarchical());
  dict->SetBoolean("invisible", !android_node->IsVisibleToUser());
  dict->SetBoolean("multiline", android_node->IsMultiLine());
  dict->SetBoolean("range", android_node->IsRangeType());
  dict->SetBoolean("password", android_node->IsPassword());
  dict->SetBoolean("scrollable", android_node->IsScrollable());
  dict->SetBoolean("selected", android_node->IsSelected());

  // String attributes.
  dict->SetString("name", android_node->GetText());

  // Int attributes.
  dict->SetInteger("item_index", android_node->GetItemIndex());
  dict->SetInteger("item_count", android_node->GetItemCount());
  dict->SetInteger("row_count", android_node->RowCount());
  dict->SetInteger("column_count", android_node->ColumnCount());
  dict->SetInteger("row_index", android_node->RowIndex());
  dict->SetInteger("row_span", android_node->RowSpan());
  dict->SetInteger("column_index", android_node->ColumnIndex());
  dict->SetInteger("column_span", android_node->ColumnSpan());
  dict->SetInteger("input_type", android_node->AndroidInputType());
  dict->SetInteger("live_region_type", android_node->AndroidLiveRegionType());
  dict->SetInteger("range_min", static_cast<int>(android_node->RangeMin()));
  dict->SetInteger("range_max", static_cast<int>(android_node->RangeMax()));
  dict->SetInteger("range_current_value",
                   static_cast<int>(android_node->RangeCurrentValue()));
}

base::string16 AccessibilityTreeFormatter::ToString(
    const base::DictionaryValue& dict,
    const base::string16& indent) {
  base::string16 line;

  base::string16 class_value;
  dict.GetString("class", &class_value);
  WriteAttribute(true, base::UTF16ToUTF8(class_value), &line);

  for (unsigned i = 0; i < arraysize(BOOL_ATTRIBUTES); i++) {
    const char* attribute_name = BOOL_ATTRIBUTES[i];
    bool value;
    if (dict.GetBoolean(attribute_name, &value) && value)
      WriteAttribute(true, attribute_name, &line);
  }

  for (unsigned i = 0; i < arraysize(STRING_ATTRIBUTES); i++) {
    const char* attribute_name = STRING_ATTRIBUTES[i];
    std::string value;
    if (!dict.GetString(attribute_name, &value) || value.empty())
      continue;
    WriteAttribute(true,
                   StringPrintf("%s='%s'", attribute_name, value.c_str()),
                   &line);
  }

  for (unsigned i = 0; i < arraysize(INT_ATTRIBUTES); i++) {
    const char* attribute_name = INT_ATTRIBUTES[i];
    int value;
    if (!dict.GetInteger(attribute_name, &value) || value == 0)
      continue;
    WriteAttribute(true,
                   StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  return indent + line + base::ASCIIToUTF16("\n");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("-actual-android.txt");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-android.txt");
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "@ANDROID-ALLOW-EMPTY:";
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return "@ANDROID-ALLOW:";
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return "@ANDROID-DENY:";
}

}  // namespace content
