// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"

#include <oleacc.h>

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "third_party/iaccessible2/ia2_api_all.h"

namespace content {
namespace {

class AccessibilityRoleStateMap {
 public:
  static AccessibilityRoleStateMap* GetInstance();

  std::map<int32, base::string16> ia_role_string_map;
  std::map<int32, base::string16> ia2_role_string_map;
  std::map<int32, base::string16> ia_state_string_map;
  std::map<int32, base::string16> ia2_state_string_map;

 private:
  AccessibilityRoleStateMap();
  virtual ~AccessibilityRoleStateMap() {}

  friend struct DefaultSingletonTraits<AccessibilityRoleStateMap>;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityRoleStateMap);
};

// static
AccessibilityRoleStateMap* AccessibilityRoleStateMap::GetInstance() {
  return Singleton<AccessibilityRoleStateMap,
                   LeakySingletonTraits<AccessibilityRoleStateMap> >::get();
}

AccessibilityRoleStateMap::AccessibilityRoleStateMap() {
// Convenience macros for generating readable strings.
#define IA_ROLE_MAP(x) ia_role_string_map[x] = L###x;  \
                       ia2_role_string_map[x] = L###x;
#define IA2_ROLE_MAP(x) ia2_role_string_map[x] = L###x;
#define IA_STATE_MAP(x) ia_state_string_map[STATE_SYSTEM_##x] = L###x;
#define IA2_STATE_MAP(x) ia2_state_string_map[x] = L###x;

  // MSAA / IAccessible roles. Each one of these is also a valid
  // IAccessible2 role, the IA_ROLE_MAP macro adds it to both.
  IA_ROLE_MAP(ROLE_SYSTEM_ALERT)
  IA_ROLE_MAP(ROLE_SYSTEM_ANIMATION)
  IA_ROLE_MAP(ROLE_SYSTEM_APPLICATION)
  IA_ROLE_MAP(ROLE_SYSTEM_BORDER)
  IA_ROLE_MAP(ROLE_SYSTEM_BUTTONDROPDOWN)
  IA_ROLE_MAP(ROLE_SYSTEM_BUTTONDROPDOWNGRID)
  IA_ROLE_MAP(ROLE_SYSTEM_BUTTONMENU)
  IA_ROLE_MAP(ROLE_SYSTEM_CARET)
  IA_ROLE_MAP(ROLE_SYSTEM_CELL)
  IA_ROLE_MAP(ROLE_SYSTEM_CHARACTER)
  IA_ROLE_MAP(ROLE_SYSTEM_CHART)
  IA_ROLE_MAP(ROLE_SYSTEM_CHECKBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_CLIENT)
  IA_ROLE_MAP(ROLE_SYSTEM_CLOCK)
  IA_ROLE_MAP(ROLE_SYSTEM_COLUMN)
  IA_ROLE_MAP(ROLE_SYSTEM_COLUMNHEADER)
  IA_ROLE_MAP(ROLE_SYSTEM_COMBOBOX)
  IA_ROLE_MAP(ROLE_SYSTEM_CURSOR)
  IA_ROLE_MAP(ROLE_SYSTEM_DIAGRAM)
  IA_ROLE_MAP(ROLE_SYSTEM_DIAL)
  IA_ROLE_MAP(ROLE_SYSTEM_DIALOG)
  IA_ROLE_MAP(ROLE_SYSTEM_DOCUMENT)
  IA_ROLE_MAP(ROLE_SYSTEM_DROPLIST)
  IA_ROLE_MAP(ROLE_SYSTEM_EQUATION)
  IA_ROLE_MAP(ROLE_SYSTEM_GRAPHIC)
  IA_ROLE_MAP(ROLE_SYSTEM_GRIP)
  IA_ROLE_MAP(ROLE_SYSTEM_GROUPING)
  IA_ROLE_MAP(ROLE_SYSTEM_HELPBALLOON)
  IA_ROLE_MAP(ROLE_SYSTEM_HOTKEYFIELD)
  IA_ROLE_MAP(ROLE_SYSTEM_INDICATOR)
  IA_ROLE_MAP(ROLE_SYSTEM_IPADDRESS)
  IA_ROLE_MAP(ROLE_SYSTEM_LINK)
  IA_ROLE_MAP(ROLE_SYSTEM_LIST)
  IA_ROLE_MAP(ROLE_SYSTEM_LISTITEM)
  IA_ROLE_MAP(ROLE_SYSTEM_MENUBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_MENUITEM)
  IA_ROLE_MAP(ROLE_SYSTEM_MENUPOPUP)
  IA_ROLE_MAP(ROLE_SYSTEM_OUTLINE)
  IA_ROLE_MAP(ROLE_SYSTEM_OUTLINEBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_OUTLINEITEM)
  IA_ROLE_MAP(ROLE_SYSTEM_PAGETAB)
  IA_ROLE_MAP(ROLE_SYSTEM_PAGETABLIST)
  IA_ROLE_MAP(ROLE_SYSTEM_PANE)
  IA_ROLE_MAP(ROLE_SYSTEM_PROGRESSBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_PROPERTYPAGE)
  IA_ROLE_MAP(ROLE_SYSTEM_PUSHBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_RADIOBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_ROW)
  IA_ROLE_MAP(ROLE_SYSTEM_ROWHEADER)
  IA_ROLE_MAP(ROLE_SYSTEM_SCROLLBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_SEPARATOR)
  IA_ROLE_MAP(ROLE_SYSTEM_SLIDER)
  IA_ROLE_MAP(ROLE_SYSTEM_SOUND)
  IA_ROLE_MAP(ROLE_SYSTEM_SPINBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_SPLITBUTTON)
  IA_ROLE_MAP(ROLE_SYSTEM_STATICTEXT)
  IA_ROLE_MAP(ROLE_SYSTEM_STATUSBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_TABLE)
  IA_ROLE_MAP(ROLE_SYSTEM_TEXT)
  IA_ROLE_MAP(ROLE_SYSTEM_TITLEBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_TOOLBAR)
  IA_ROLE_MAP(ROLE_SYSTEM_TOOLTIP)
  IA_ROLE_MAP(ROLE_SYSTEM_WHITESPACE)
  IA_ROLE_MAP(ROLE_SYSTEM_WINDOW)

  // IAccessible2 roles.
  IA2_ROLE_MAP(IA2_ROLE_CANVAS)
  IA2_ROLE_MAP(IA2_ROLE_CAPTION)
  IA2_ROLE_MAP(IA2_ROLE_CHECK_MENU_ITEM)
  IA2_ROLE_MAP(IA2_ROLE_COLOR_CHOOSER)
  IA2_ROLE_MAP(IA2_ROLE_DATE_EDITOR)
  IA2_ROLE_MAP(IA2_ROLE_DESKTOP_ICON)
  IA2_ROLE_MAP(IA2_ROLE_DESKTOP_PANE)
  IA2_ROLE_MAP(IA2_ROLE_DIRECTORY_PANE)
  IA2_ROLE_MAP(IA2_ROLE_EDITBAR)
  IA2_ROLE_MAP(IA2_ROLE_EMBEDDED_OBJECT)
  IA2_ROLE_MAP(IA2_ROLE_ENDNOTE)
  IA2_ROLE_MAP(IA2_ROLE_FILE_CHOOSER)
  IA2_ROLE_MAP(IA2_ROLE_FONT_CHOOSER)
  IA2_ROLE_MAP(IA2_ROLE_FOOTER)
  IA2_ROLE_MAP(IA2_ROLE_FOOTNOTE)
  IA2_ROLE_MAP(IA2_ROLE_FORM)
  IA2_ROLE_MAP(IA2_ROLE_FRAME)
  IA2_ROLE_MAP(IA2_ROLE_GLASS_PANE)
  IA2_ROLE_MAP(IA2_ROLE_HEADER)
  IA2_ROLE_MAP(IA2_ROLE_HEADING)
  IA2_ROLE_MAP(IA2_ROLE_ICON)
  IA2_ROLE_MAP(IA2_ROLE_IMAGE_MAP)
  IA2_ROLE_MAP(IA2_ROLE_INPUT_METHOD_WINDOW)
  IA2_ROLE_MAP(IA2_ROLE_INTERNAL_FRAME)
  IA2_ROLE_MAP(IA2_ROLE_LABEL)
  IA2_ROLE_MAP(IA2_ROLE_LAYERED_PANE)
  IA2_ROLE_MAP(IA2_ROLE_NOTE)
  IA2_ROLE_MAP(IA2_ROLE_OPTION_PANE)
  IA2_ROLE_MAP(IA2_ROLE_PAGE)
  IA2_ROLE_MAP(IA2_ROLE_PARAGRAPH)
  IA2_ROLE_MAP(IA2_ROLE_RADIO_MENU_ITEM)
  IA2_ROLE_MAP(IA2_ROLE_REDUNDANT_OBJECT)
  IA2_ROLE_MAP(IA2_ROLE_ROOT_PANE)
  IA2_ROLE_MAP(IA2_ROLE_RULER)
  IA2_ROLE_MAP(IA2_ROLE_SCROLL_PANE)
  IA2_ROLE_MAP(IA2_ROLE_SECTION)
  IA2_ROLE_MAP(IA2_ROLE_SHAPE)
  IA2_ROLE_MAP(IA2_ROLE_SPLIT_PANE)
  IA2_ROLE_MAP(IA2_ROLE_TEAR_OFF_MENU)
  IA2_ROLE_MAP(IA2_ROLE_TERMINAL)
  IA2_ROLE_MAP(IA2_ROLE_TEXT_FRAME)
  IA2_ROLE_MAP(IA2_ROLE_TOGGLE_BUTTON)
  IA2_ROLE_MAP(IA2_ROLE_UNKNOWN)
  IA2_ROLE_MAP(IA2_ROLE_VIEW_PORT)

  // MSAA / IAccessible states. Unlike roles, these are not also IA2 states.
  IA_STATE_MAP(ALERT_HIGH)
  IA_STATE_MAP(ALERT_LOW)
  IA_STATE_MAP(ALERT_MEDIUM)
  IA_STATE_MAP(ANIMATED)
  IA_STATE_MAP(BUSY)
  IA_STATE_MAP(CHECKED)
  IA_STATE_MAP(COLLAPSED)
  IA_STATE_MAP(DEFAULT)
  IA_STATE_MAP(EXPANDED)
  IA_STATE_MAP(EXTSELECTABLE)
  IA_STATE_MAP(FLOATING)
  IA_STATE_MAP(FOCUSABLE)
  IA_STATE_MAP(FOCUSED)
  IA_STATE_MAP(HASPOPUP)
  IA_STATE_MAP(HOTTRACKED)
  IA_STATE_MAP(INVISIBLE)
  IA_STATE_MAP(LINKED)
  IA_STATE_MAP(MARQUEED)
  IA_STATE_MAP(MIXED)
  IA_STATE_MAP(MOVEABLE)
  IA_STATE_MAP(MULTISELECTABLE)
  IA_STATE_MAP(OFFSCREEN)
  IA_STATE_MAP(PRESSED)
  IA_STATE_MAP(PROTECTED)
  IA_STATE_MAP(READONLY)
  IA_STATE_MAP(SELECTABLE)
  IA_STATE_MAP(SELECTED)
  IA_STATE_MAP(SELFVOICING)
  IA_STATE_MAP(SIZEABLE)
  IA_STATE_MAP(TRAVERSED)
  IA_STATE_MAP(UNAVAILABLE)

  // IAccessible2 states.
  IA2_STATE_MAP(IA2_STATE_ACTIVE)
  IA2_STATE_MAP(IA2_STATE_ARMED)
  IA2_STATE_MAP(IA2_STATE_DEFUNCT)
  IA2_STATE_MAP(IA2_STATE_EDITABLE)
  IA2_STATE_MAP(IA2_STATE_ICONIFIED)
  IA2_STATE_MAP(IA2_STATE_INVALID_ENTRY)
  IA2_STATE_MAP(IA2_STATE_MANAGES_DESCENDANTS)
  IA2_STATE_MAP(IA2_STATE_MODAL)
  IA2_STATE_MAP(IA2_STATE_MULTI_LINE)
  IA2_STATE_MAP(IA2_STATE_REQUIRED)
  IA2_STATE_MAP(IA2_STATE_SELECTABLE_TEXT)
  IA2_STATE_MAP(IA2_STATE_SINGLE_LINE)
  IA2_STATE_MAP(IA2_STATE_STALE)
  IA2_STATE_MAP(IA2_STATE_SUPPORTS_AUTOCOMPLETION)
  IA2_STATE_MAP(IA2_STATE_TRANSIENT)

  // Untested states include those that would be repeated on nearly every node,
  // or would vary based on window size.
  // IA2_STATE_MAP(IA2_STATE_HORIZONTAL) // Untested.
  // IA2_STATE_MAP(IA2_STATE_OPAQUE) // Untested.
  // IA2_STATE_MAP(IA2_STATE_VERTICAL) // Untested.
}

}  // namespace.

base::string16 IAccessibleRoleToString(int32 ia_role) {
  return AccessibilityRoleStateMap::GetInstance()->ia_role_string_map[ia_role];
}

base::string16 IAccessible2RoleToString(int32 ia_role) {
  return AccessibilityRoleStateMap::GetInstance()->ia2_role_string_map[ia_role];
}

void IAccessibleStateToStringVector(int32 ia_state,
                                    std::vector<base::string16>* result) {
  const std::map<int32, base::string16>& state_string_map =
      AccessibilityRoleStateMap::GetInstance()->ia_state_string_map;
  std::map<int32, base::string16>::const_iterator it;
  for (it = state_string_map.begin(); it != state_string_map.end(); ++it) {
    if (it->first & ia_state)
      result->push_back(it->second);
  }
}

base::string16 IAccessibleStateToString(int32 ia_state) {
  std::vector<base::string16> strings;
  IAccessibleStateToStringVector(ia_state, &strings);
  return JoinString(strings, ',');
}

void IAccessible2StateToStringVector(int32 ia2_state,
                                     std::vector<base::string16>* result) {
  const std::map<int32, base::string16>& state_string_map =
      AccessibilityRoleStateMap::GetInstance()->ia2_state_string_map;
  std::map<int32, base::string16>::const_iterator it;
  for (it = state_string_map.begin(); it != state_string_map.end(); ++it) {
    if (it->first & ia2_state)
      result->push_back(it->second);
  }
}

base::string16 IAccessible2StateToString(int32 ia2_state) {
  std::vector<base::string16> strings;
  IAccessible2StateToStringVector(ia2_state, &strings);
  return JoinString(strings, ',');
}

}  // namespace content
