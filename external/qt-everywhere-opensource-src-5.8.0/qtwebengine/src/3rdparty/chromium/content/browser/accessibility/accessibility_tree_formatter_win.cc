// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"


namespace content {

class AccessibilityTreeFormatterWin : public AccessibilityTreeFormatter {
 public:
  AccessibilityTreeFormatterWin();
  ~AccessibilityTreeFormatterWin() override;

 private:
  const base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict) override;
  base::string16 ToString(const base::DictionaryValue& node) override;
};

// static
AccessibilityTreeFormatter* AccessibilityTreeFormatter::Create() {
  return new AccessibilityTreeFormatterWin();
}

AccessibilityTreeFormatterWin::AccessibilityTreeFormatterWin() {
  ui::win::CreateATLModuleIfNeeded();
}

AccessibilityTreeFormatterWin::~AccessibilityTreeFormatterWin() {
}

const char* const ALL_ATTRIBUTES[] = {
  "name",
  "value",
  "states",
  "attributes",
  "text_attributes",
  "role_name",
  "ia2_hypertext",
  "currentValue",
  "minimumValue",
  "maximumValue",
  "description",
  "default_action",
  "keyboard_shortcut",
  "location",
  "size",
  "index_in_parent",
  "n_relations",
  "group_level",
  "similar_items_in_group",
  "position_in_group",
  "table_rows",
  "table_columns",
  "row_index",
  "column_index",
  "n_characters",
  "caret_offset",
  "n_selections",
  "selection_start",
  "selection_end"
};

namespace {

base::string16 GetIA2Hypertext(BrowserAccessibilityWin& ax_object) {
  base::win::ScopedBstr text_bstr;
  HRESULT hr;

  hr = ax_object.get_text(0, IA2_TEXT_OFFSET_LENGTH, text_bstr.Receive());
  if (FAILED(hr))
    return base::string16();

  base::string16 ia2_hypertext(text_bstr, text_bstr.Length());
  // IA2 Spec calls embedded objects hyperlinks. We stick to embeds for clarity.
  LONG number_of_embeds;
  hr = ax_object.get_nHyperlinks(&number_of_embeds);
  if (FAILED(hr) || number_of_embeds == 0)
    return ia2_hypertext;

  // Replace all embedded characters with the child indices of the accessibility
  // objects they refer to.
  base::string16 embedded_character(1,
      BrowserAccessibilityWin::kEmbeddedCharacter);
  size_t character_index = 0;
  size_t hypertext_index = 0;
  while (hypertext_index < ia2_hypertext.length()) {
    if (ia2_hypertext[hypertext_index] !=
        BrowserAccessibilityWin::kEmbeddedCharacter) {
      ++character_index;
      ++hypertext_index;
      continue;
    }

    LONG index_of_embed;
    hr = ax_object.get_hyperlinkIndex(character_index, &index_of_embed);
    // S_FALSE will be returned if no embedded object is found at the given
    // embedded character offset. Exclude child index from such cases.
    LONG child_index = -1;
    if (hr == S_OK) {
      DCHECK_GE(index_of_embed, 0);
      base::win::ScopedComPtr<IAccessibleHyperlink> embedded_object;
      hr = ax_object.get_hyperlink(
          index_of_embed, embedded_object.Receive());
      DCHECK(SUCCEEDED(hr));
      base::win::ScopedComPtr<IAccessible2> ax_embed;
      hr = embedded_object.QueryInterface(ax_embed.Receive());
      DCHECK(SUCCEEDED(hr));
      hr = ax_embed->get_indexInParent(&child_index);
      DCHECK(SUCCEEDED(hr));
    }

    base::string16 child_index_str(L"<obj");
    if (child_index >= 0) {
      base::StringAppendF(&child_index_str, L"%d>", child_index);
    } else {
      base::StringAppendF(&child_index_str, L">");
    }
    base::ReplaceFirstSubstringAfterOffset(&ia2_hypertext, hypertext_index,
        embedded_character, child_index_str);
    ++character_index;
    hypertext_index += child_index_str.length();
    --number_of_embeds;
  }
  DCHECK_EQ(number_of_embeds, 0);

  return ia2_hypertext;
}

}  // namespace

void AccessibilityTreeFormatterWin::AddProperties(
    const BrowserAccessibility& node, base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
  BrowserAccessibilityWin* ax_object =
      ToBrowserAccessibilityWin(const_cast<BrowserAccessibility*>(&node));
  DCHECK(ax_object);

  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;

  dict->SetString("role", IAccessible2RoleToString(ax_object->ia2_role()));

  base::win::ScopedBstr temp_bstr;
  if (SUCCEEDED(ax_object->get_accName(variant_self, temp_bstr.Receive()))) {
    base::string16 name = base::string16(temp_bstr, temp_bstr.Length());

    // Ignore a JAWS workaround where the name of a document is " ".
    if (name != L" " || ax_object->ia2_role() != ROLE_SYSTEM_DOCUMENT)
      dict->SetString("name", name);
  }
  temp_bstr.Reset();

  if (SUCCEEDED(ax_object->get_accValue(variant_self, temp_bstr.Receive())))
    dict->SetString("value", base::string16(temp_bstr, temp_bstr.Length()));
  temp_bstr.Reset();

  std::vector<base::string16> state_strings;
  int32_t ia_state = ax_object->ia_state();

  // Avoid flakiness: these states depend on whether the window is focused
  // and the position of the mouse cursor.
  ia_state &= ~STATE_SYSTEM_HOTTRACKED;
  ia_state &= ~STATE_SYSTEM_OFFSCREEN;

  IAccessibleStateToStringVector(ia_state, &state_strings);
  IAccessible2StateToStringVector(ax_object->ia2_state(), &state_strings);
  std::unique_ptr<base::ListValue> states(new base::ListValue());
  for (const base::string16& state_string : state_strings)
    states->AppendString(base::UTF16ToUTF8(state_string));
  dict->Set("states", std::move(states));

  const std::vector<base::string16>& ia2_attributes =
      ax_object->ia2_attributes();
  std::unique_ptr<base::ListValue> attributes(new base::ListValue());
  for (const base::string16& ia2_attribute : ia2_attributes)
    attributes->AppendString(base::UTF16ToUTF8(ia2_attribute));
  dict->Set("attributes", std::move(attributes));

  ax_object->ComputeStylesIfNeeded();
  const std::map<int, std::vector<base::string16>>& ia2_text_attributes =
      ax_object->offset_to_text_attributes();
  std::unique_ptr<base::ListValue> text_attributes(new base::ListValue());
  for (const auto& style_span : ia2_text_attributes) {
    int start_offset = style_span.first;
    text_attributes->AppendString("offset:" + base::IntToString(start_offset));
    for (const base::string16& text_attribute : style_span.second)
      text_attributes->AppendString(base::UTF16ToUTF8(text_attribute));
  }
  dict->Set("text_attributes", std::move(text_attributes));

  dict->SetString("role_name", ax_object->role_name());
  dict->SetString("ia2_hypertext", GetIA2Hypertext(*ax_object));

  VARIANT currentValue;
  if (ax_object->get_currentValue(&currentValue) == S_OK)
    dict->SetDouble("currentValue", V_R8(&currentValue));

  VARIANT minimumValue;
  if (ax_object->get_minimumValue(&minimumValue) == S_OK)
    dict->SetDouble("minimumValue", V_R8(&minimumValue));

  VARIANT maximumValue;
  if (ax_object->get_maximumValue(&maximumValue) == S_OK)
    dict->SetDouble("maximumValue", V_R8(&maximumValue));

  if (SUCCEEDED(ax_object->get_accDescription(variant_self,
      temp_bstr.Receive()))) {
    dict->SetString("description", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  if (SUCCEEDED(ax_object->get_accDefaultAction(variant_self,
      temp_bstr.Receive()))) {
    dict->SetString("default_action", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  if (SUCCEEDED(
      ax_object->get_accKeyboardShortcut(variant_self, temp_bstr.Receive()))) {
    dict->SetString("keyboard_shortcut", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  if (SUCCEEDED(ax_object->get_accHelp(variant_self, temp_bstr.Receive())))
    dict->SetString("help", base::string16(temp_bstr, temp_bstr.Length()));
  temp_bstr.Reset();

  BrowserAccessibility* root = node.manager()->GetRootManager()->GetRoot();
  LONG left, top, width, height;
  LONG root_left, root_top, root_width, root_height;
  if (SUCCEEDED(ax_object->accLocation(
          &left, &top, &width, &height, variant_self)) &&
      SUCCEEDED(ToBrowserAccessibilityWin(root)->accLocation(
          &root_left, &root_top, &root_width, &root_height, variant_self))) {
    base::DictionaryValue* location = new base::DictionaryValue;
    location->SetInteger("x", left - root_left);
    location->SetInteger("y", top - root_top);
    dict->Set("location", location);

    base::DictionaryValue* size = new base::DictionaryValue;
    size->SetInteger("width", width);
    size->SetInteger("height", height);
    dict->Set("size", size);
  }

  LONG index_in_parent;
  if (SUCCEEDED(ax_object->get_indexInParent(&index_in_parent)))
    dict->SetInteger("index_in_parent", index_in_parent);

  LONG n_relations;
  if (SUCCEEDED(ax_object->get_nRelations(&n_relations)))
    dict->SetInteger("n_relations", n_relations);

  LONG group_level, similar_items_in_group, position_in_group;
  if (SUCCEEDED(ax_object->get_groupPosition(&group_level,
                                 &similar_items_in_group,
                                 &position_in_group))) {
    dict->SetInteger("group_level", group_level);
    dict->SetInteger("similar_items_in_group", similar_items_in_group);
    dict->SetInteger("position_in_group", position_in_group);
  }

  LONG table_rows;
  if (SUCCEEDED(ax_object->get_nRows(&table_rows)))
    dict->SetInteger("table_rows", table_rows);

  LONG table_columns;
  if (SUCCEEDED(ax_object->get_nRows(&table_columns)))
    dict->SetInteger("table_columns", table_columns);

  LONG row_index;
  if (SUCCEEDED(ax_object->get_rowIndex(&row_index)))
    dict->SetInteger("row_index", row_index);

  LONG column_index;
  if (SUCCEEDED(ax_object->get_columnIndex(&column_index)))
    dict->SetInteger("column_index", column_index);

  LONG n_characters;
  if (SUCCEEDED(ax_object->get_nCharacters(&n_characters)))
    dict->SetInteger("n_characters", n_characters);

  LONG caret_offset;
  if (ax_object->get_caretOffset(&caret_offset) == S_OK)
    dict->SetInteger("caret_offset", caret_offset);

  LONG n_selections;
  if (SUCCEEDED(ax_object->get_nSelections(&n_selections))) {
    dict->SetInteger("n_selections", n_selections);
    if (n_selections > 0) {
      LONG start, end;
      if (SUCCEEDED(ax_object->get_selection(0, &start, &end))) {
        dict->SetInteger("selection_start", start);
        dict->SetInteger("selection_end", end);
      }
    }
  }
}

base::string16 AccessibilityTreeFormatterWin::ToString(
    const base::DictionaryValue& dict) {
  base::string16 line;

  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
    WriteAttribute(true, base::IntToString16(id_value), &line);
  }

  base::string16 role_value;
  dict.GetString("role", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  for (const char* attribute_name : ALL_ATTRIBUTES) {
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->GetType()) {
      case base::Value::TYPE_STRING: {
        base::string16 string_value;
        value->GetAsString(&string_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls='%ls'",
                                    base::UTF8ToUTF16(attribute_name).c_str(),
                                    string_value.c_str()),
                       &line);
        break;
      }
      case base::Value::TYPE_INTEGER: {
        int int_value = 0;
        value->GetAsInteger(&int_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%d",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          int_value),
                       &line);
        break;
      }
      case base::Value::TYPE_DOUBLE: {
        double double_value = 0.0;
        value->GetAsDouble(&double_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%.2f",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          double_value),
                       &line);
        break;
      }
      case base::Value::TYPE_LIST: {
        // Currently all list values are string and are written without
        // attribute names.
        const base::ListValue* list_value;
        value->GetAsList(&list_value);
        for (base::ListValue::const_iterator it = list_value->begin();
             it != list_value->end();
             ++it) {
          base::string16 string_value;
          if ((*it)->GetAsString(&string_value))
            WriteAttribute(false, string_value, &line);
        }
        break;
      }
      case base::Value::TYPE_DICTIONARY: {
        // Currently all dictionary values are coordinates.
        // Revisit this if that changes.
        const base::DictionaryValue* dict_value;
        value->GetAsDictionary(&dict_value);
        if (strcmp(attribute_name, "size") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("size", "width", "height",
                                           *dict_value),
                         &line);
        } else if (strcmp(attribute_name, "location") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("location", "x", "y", *dict_value),
                         &line);
        }
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  return line;
}

const base::FilePath::StringType
AccessibilityTreeFormatterWin::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-win.txt");
}

const std::string AccessibilityTreeFormatterWin::GetAllowEmptyString() {
  return "@WIN-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterWin::GetAllowString() {
  return "@WIN-ALLOW:";
}

const std::string AccessibilityTreeFormatterWin::GetDenyString() {
  return "@WIN-DENY:";
}

}  // namespace content
