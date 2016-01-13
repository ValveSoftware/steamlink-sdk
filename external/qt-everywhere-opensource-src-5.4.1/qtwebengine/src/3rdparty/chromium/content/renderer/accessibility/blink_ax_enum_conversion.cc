// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_enum_conversion.h"

#include "base/logging.h"

namespace content {

uint32 AXStateFromBlink(const blink::WebAXObject& o) {
  uint32 state = 0;
  if (o.isChecked())
    state |= (1 << ui::AX_STATE_CHECKED);

  if (o.isCollapsed())
    state |= (1 << ui::AX_STATE_COLLAPSED);

  if (o.canSetFocusAttribute())
    state |= (1 << ui::AX_STATE_FOCUSABLE);

  if (o.isFocused())
    state |= (1 << ui::AX_STATE_FOCUSED);

  if (o.role() == blink::WebAXRolePopUpButton ||
      o.ariaHasPopup()) {
    state |= (1 << ui::AX_STATE_HASPOPUP);
    if (!o.isCollapsed())
      state |= (1 << ui::AX_STATE_EXPANDED);
  }

  if (o.isHovered())
    state |= (1 << ui::AX_STATE_HOVERED);

  if (o.isIndeterminate())
    state |= (1 << ui::AX_STATE_INDETERMINATE);

  if (!o.isVisible())
    state |= (1 << ui::AX_STATE_INVISIBLE);

  if (o.isLinked())
    state |= (1 << ui::AX_STATE_LINKED);

  if (o.isMultiSelectable())
    state |= (1 << ui::AX_STATE_MULTISELECTABLE);

  if (o.isOffScreen())
    state |= (1 << ui::AX_STATE_OFFSCREEN);

  if (o.isPressed())
    state |= (1 << ui::AX_STATE_PRESSED);

  if (o.isPasswordField())
    state |= (1 << ui::AX_STATE_PROTECTED);

  if (o.isReadOnly())
    state |= (1 << ui::AX_STATE_READ_ONLY);

  if (o.isRequired())
    state |= (1 << ui::AX_STATE_REQUIRED);

  if (o.canSetSelectedAttribute())
    state |= (1 << ui::AX_STATE_SELECTABLE);

  if (o.isSelected())
    state |= (1 << ui::AX_STATE_SELECTED);

  if (o.isVisited())
    state |= (1 << ui::AX_STATE_VISITED);

  if (o.isEnabled())
    state |= (1 << ui::AX_STATE_ENABLED);

  if (o.isVertical())
    state |= (1 << ui::AX_STATE_VERTICAL);

  if (o.isVisited())
    state |= (1 << ui::AX_STATE_VISITED);

  return state;
}

ui::AXRole AXRoleFromBlink(blink::WebAXRole role) {
  switch (role) {
    case blink::WebAXRoleAlert:
      return ui::AX_ROLE_ALERT;
    case blink::WebAXRoleAlertDialog:
      return ui::AX_ROLE_ALERT_DIALOG;
    case blink::WebAXRoleAnnotation:
      return ui::AX_ROLE_ANNOTATION;
    case blink::WebAXRoleApplication:
      return ui::AX_ROLE_APPLICATION;
    case blink::WebAXRoleArticle:
      return ui::AX_ROLE_ARTICLE;
    case blink::WebAXRoleBanner:
      return ui::AX_ROLE_BANNER;
    case blink::WebAXRoleBrowser:
      return ui::AX_ROLE_BROWSER;
    case blink::WebAXRoleBusyIndicator:
      return ui::AX_ROLE_BUSY_INDICATOR;
    case blink::WebAXRoleButton:
      return ui::AX_ROLE_BUTTON;
    case blink::WebAXRoleCanvas:
      return ui::AX_ROLE_CANVAS;
    case blink::WebAXRoleCell:
      return ui::AX_ROLE_CELL;
    case blink::WebAXRoleCheckBox:
      return ui::AX_ROLE_CHECK_BOX;
    case blink::WebAXRoleColorWell:
      return ui::AX_ROLE_COLOR_WELL;
    case blink::WebAXRoleColumn:
      return ui::AX_ROLE_COLUMN;
    case blink::WebAXRoleColumnHeader:
      return ui::AX_ROLE_COLUMN_HEADER;
    case blink::WebAXRoleComboBox:
      return ui::AX_ROLE_COMBO_BOX;
    case blink::WebAXRoleComplementary:
      return ui::AX_ROLE_COMPLEMENTARY;
    case blink::WebAXRoleContentInfo:
      return ui::AX_ROLE_CONTENT_INFO;
    case blink::WebAXRoleDefinition:
      return ui::AX_ROLE_DEFINITION;
    case blink::WebAXRoleDescriptionListDetail:
      return ui::AX_ROLE_DESCRIPTION_LIST_DETAIL;
    case blink::WebAXRoleDescriptionListTerm:
      return ui::AX_ROLE_DESCRIPTION_LIST_TERM;
    case blink::WebAXRoleDialog:
      return ui::AX_ROLE_DIALOG;
    case blink::WebAXRoleDirectory:
      return ui::AX_ROLE_DIRECTORY;
    case blink::WebAXRoleDisclosureTriangle:
      return ui::AX_ROLE_DISCLOSURE_TRIANGLE;
    case blink::WebAXRoleDiv:
      return ui::AX_ROLE_DIV;
    case blink::WebAXRoleDocument:
      return ui::AX_ROLE_DOCUMENT;
    case blink::WebAXRoleDrawer:
      return ui::AX_ROLE_DRAWER;
    case blink::WebAXRoleEditableText:
      return ui::AX_ROLE_EDITABLE_TEXT;
    case blink::WebAXRoleEmbeddedObject:
      return ui::AX_ROLE_EMBEDDED_OBJECT;
    case blink::WebAXRoleFooter:
      return ui::AX_ROLE_FOOTER;
    case blink::WebAXRoleForm:
      return ui::AX_ROLE_FORM;
    case blink::WebAXRoleGrid:
      return ui::AX_ROLE_GRID;
    case blink::WebAXRoleGroup:
      return ui::AX_ROLE_GROUP;
    case blink::WebAXRoleGrowArea:
      return ui::AX_ROLE_GROW_AREA;
    case blink::WebAXRoleHeading:
      return ui::AX_ROLE_HEADING;
    case blink::WebAXRoleHelpTag:
      return ui::AX_ROLE_HELP_TAG;
    case blink::WebAXRoleHorizontalRule:
      return ui::AX_ROLE_HORIZONTAL_RULE;
    case blink::WebAXRoleIframe:
      return ui::AX_ROLE_IFRAME;
    case blink::WebAXRoleIgnored:
      return ui::AX_ROLE_IGNORED;
    case blink::WebAXRoleImage:
      return ui::AX_ROLE_IMAGE;
    case blink::WebAXRoleImageMap:
      return ui::AX_ROLE_IMAGE_MAP;
    case blink::WebAXRoleImageMapLink:
      return ui::AX_ROLE_IMAGE_MAP_LINK;
    case blink::WebAXRoleIncrementor:
      return ui::AX_ROLE_INCREMENTOR;
    case blink::WebAXRoleInlineTextBox:
      return ui::AX_ROLE_INLINE_TEXT_BOX;
    case blink::WebAXRoleLabel:
      return ui::AX_ROLE_LABEL_TEXT;
    case blink::WebAXRoleLegend:
      return ui::AX_ROLE_LEGEND;
    case blink::WebAXRoleLink:
      return ui::AX_ROLE_LINK;
    case blink::WebAXRoleList:
      return ui::AX_ROLE_LIST;
    case blink::WebAXRoleListBox:
      return ui::AX_ROLE_LIST_BOX;
    case blink::WebAXRoleListBoxOption:
      return ui::AX_ROLE_LIST_BOX_OPTION;
    case blink::WebAXRoleListItem:
      return ui::AX_ROLE_LIST_ITEM;
    case blink::WebAXRoleListMarker:
      return ui::AX_ROLE_LIST_MARKER;
    case blink::WebAXRoleLog:
      return ui::AX_ROLE_LOG;
    case blink::WebAXRoleMain:
      return ui::AX_ROLE_MAIN;
    case blink::WebAXRoleMarquee:
      return ui::AX_ROLE_MARQUEE;
    case blink::WebAXRoleMath:
      return ui::AX_ROLE_MATH;
    case blink::WebAXRoleMathElement:
      return ui::AX_ROLE_MATH_ELEMENT;
    case blink::WebAXRoleMatte:
      return ui::AX_ROLE_MATTE;
    case blink::WebAXRoleMenu:
      return ui::AX_ROLE_MENU;
    case blink::WebAXRoleMenuBar:
      return ui::AX_ROLE_MENU_BAR;
    case blink::WebAXRoleMenuButton:
      return ui::AX_ROLE_MENU_BUTTON;
    case blink::WebAXRoleMenuItem:
      return ui::AX_ROLE_MENU_ITEM;
    case blink::WebAXRoleMenuListOption:
      return ui::AX_ROLE_MENU_LIST_OPTION;
    case blink::WebAXRoleMenuListPopup:
      return ui::AX_ROLE_MENU_LIST_POPUP;
    case blink::WebAXRoleNavigation:
      return ui::AX_ROLE_NAVIGATION;
    case blink::WebAXRoleNote:
      return ui::AX_ROLE_NOTE;
    case blink::WebAXRoleOutline:
      return ui::AX_ROLE_OUTLINE;
    case blink::WebAXRoleParagraph:
      return ui::AX_ROLE_PARAGRAPH;
    case blink::WebAXRolePopUpButton:
      return ui::AX_ROLE_POP_UP_BUTTON;
    case blink::WebAXRolePresentational:
      return ui::AX_ROLE_PRESENTATIONAL;
    case blink::WebAXRoleProgressIndicator:
      return ui::AX_ROLE_PROGRESS_INDICATOR;
    case blink::WebAXRoleRadioButton:
      return ui::AX_ROLE_RADIO_BUTTON;
    case blink::WebAXRoleRadioGroup:
      return ui::AX_ROLE_RADIO_GROUP;
    case blink::WebAXRoleRegion:
      return ui::AX_ROLE_REGION;
    case blink::WebAXRoleRootWebArea:
      return ui::AX_ROLE_ROOT_WEB_AREA;
    case blink::WebAXRoleRow:
      return ui::AX_ROLE_ROW;
    case blink::WebAXRoleRowHeader:
      return ui::AX_ROLE_ROW_HEADER;
    case blink::WebAXRoleRuler:
      return ui::AX_ROLE_RULER;
    case blink::WebAXRoleRulerMarker:
      return ui::AX_ROLE_RULER_MARKER;
    case blink::WebAXRoleSVGRoot:
      return ui::AX_ROLE_SVG_ROOT;
    case blink::WebAXRoleScrollArea:
      return ui::AX_ROLE_SCROLL_AREA;
    case blink::WebAXRoleScrollBar:
      return ui::AX_ROLE_SCROLL_BAR;
    case blink::WebAXRoleSeamlessWebArea:
      return ui::AX_ROLE_SEAMLESS_WEB_AREA;
    case blink::WebAXRoleSearch:
      return ui::AX_ROLE_SEARCH;
    case blink::WebAXRoleSheet:
      return ui::AX_ROLE_SHEET;
    case blink::WebAXRoleSlider:
      return ui::AX_ROLE_SLIDER;
    case blink::WebAXRoleSliderThumb:
      return ui::AX_ROLE_SLIDER_THUMB;
    case blink::WebAXRoleSpinButton:
      return ui::AX_ROLE_SPIN_BUTTON;
    case blink::WebAXRoleSpinButtonPart:
      return ui::AX_ROLE_SPIN_BUTTON_PART;
    case blink::WebAXRoleSplitGroup:
      return ui::AX_ROLE_SPLIT_GROUP;
    case blink::WebAXRoleSplitter:
      return ui::AX_ROLE_SPLITTER;
    case blink::WebAXRoleStaticText:
      return ui::AX_ROLE_STATIC_TEXT;
    case blink::WebAXRoleStatus:
      return ui::AX_ROLE_STATUS;
    case blink::WebAXRoleSystemWide:
      return ui::AX_ROLE_SYSTEM_WIDE;
    case blink::WebAXRoleTab:
      return ui::AX_ROLE_TAB;
    case blink::WebAXRoleTabGroup:
      return ui::AX_ROLE_TAB_GROUP;
    case blink::WebAXRoleTabList:
      return ui::AX_ROLE_TAB_LIST;
    case blink::WebAXRoleTabPanel:
      return ui::AX_ROLE_TAB_PANEL;
    case blink::WebAXRoleTable:
      return ui::AX_ROLE_TABLE;
    case blink::WebAXRoleTableHeaderContainer:
      return ui::AX_ROLE_TABLE_HEADER_CONTAINER;
    case blink::WebAXRoleTextArea:
      return ui::AX_ROLE_TEXT_AREA;
    case blink::WebAXRoleTextField:
      return ui::AX_ROLE_TEXT_FIELD;
    case blink::WebAXRoleTimer:
      return ui::AX_ROLE_TIMER;
    case blink::WebAXRoleToggleButton:
      return ui::AX_ROLE_TOGGLE_BUTTON;
    case blink::WebAXRoleToolbar:
      return ui::AX_ROLE_TOOLBAR;
    case blink::WebAXRoleTree:
      return ui::AX_ROLE_TREE;
    case blink::WebAXRoleTreeGrid:
      return ui::AX_ROLE_TREE_GRID;
    case blink::WebAXRoleTreeItem:
      return ui::AX_ROLE_TREE_ITEM;
    case blink::WebAXRoleUnknown:
      return ui::AX_ROLE_UNKNOWN;
    case blink::WebAXRoleUserInterfaceTooltip:
      return ui::AX_ROLE_TOOLTIP;
    case blink::WebAXRoleValueIndicator:
      return ui::AX_ROLE_VALUE_INDICATOR;
    case blink::WebAXRoleWebArea:
      return ui::AX_ROLE_WEB_AREA;
    case blink::WebAXRoleWindow:
      return ui::AX_ROLE_WINDOW;
    default:
      // We can't add an assertion here, that prevents us
      // from adding new role enums in Blink.
      return static_cast<ui::AXRole>(-1);
  }
}

ui::AXEvent AXEventFromBlink(blink::WebAXEvent event) {
  switch (event) {
    case blink::WebAXEventActiveDescendantChanged:
      return ui::AX_EVENT_ACTIVEDESCENDANTCHANGED;
    case blink::WebAXEventAlert:
      return ui::AX_EVENT_ALERT;
    case blink::WebAXEventAriaAttributeChanged:
      return ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED;
    case blink::WebAXEventAutocorrectionOccured:
      return ui::AX_EVENT_AUTOCORRECTION_OCCURED;
    case blink::WebAXEventBlur:
      return ui::AX_EVENT_BLUR;
    case blink::WebAXEventCheckedStateChanged:
      return ui::AX_EVENT_CHECKED_STATE_CHANGED;
    case blink::WebAXEventChildrenChanged:
      return ui::AX_EVENT_CHILDREN_CHANGED;
    case blink::WebAXEventFocus:
      return ui::AX_EVENT_FOCUS;
    case blink::WebAXEventHide:
      return ui::AX_EVENT_HIDE;
    case blink::WebAXEventInvalidStatusChanged:
      return ui::AX_EVENT_INVALID_STATUS_CHANGED;
    case blink::WebAXEventLayoutComplete:
      return ui::AX_EVENT_LAYOUT_COMPLETE;
    case blink::WebAXEventLiveRegionChanged:
      return ui::AX_EVENT_LIVE_REGION_CHANGED;
    case blink::WebAXEventLoadComplete:
      return ui::AX_EVENT_LOAD_COMPLETE;
    case blink::WebAXEventLocationChanged:
      return ui::AX_EVENT_LOCATION_CHANGED;
    case blink::WebAXEventMenuListItemSelected:
      return ui::AX_EVENT_MENU_LIST_ITEM_SELECTED;
    case blink::WebAXEventMenuListValueChanged:
        return ui::AX_EVENT_MENU_LIST_VALUE_CHANGED;
    case blink::WebAXEventRowCollapsed:
      return ui::AX_EVENT_ROW_COLLAPSED;
    case blink::WebAXEventRowCountChanged:
      return ui::AX_EVENT_ROW_COUNT_CHANGED;
    case blink::WebAXEventRowExpanded:
      return ui::AX_EVENT_ROW_EXPANDED;
    case blink::WebAXEventScrollPositionChanged:
      return ui::AX_EVENT_SCROLL_POSITION_CHANGED;
    case blink::WebAXEventScrolledToAnchor:
      return ui::AX_EVENT_SCROLLED_TO_ANCHOR;
    case blink::WebAXEventSelectedChildrenChanged:
      return ui::AX_EVENT_SELECTED_CHILDREN_CHANGED;
    case blink::WebAXEventSelectedTextChanged:
      return ui::AX_EVENT_SELECTED_TEXT_CHANGED;
    case blink::WebAXEventShow:
      return ui::AX_EVENT_SHOW;
    case blink::WebAXEventTextChanged:
      return ui::AX_EVENT_TEXT_CHANGED;
    case blink::WebAXEventTextInserted:
      return ui::AX_EVENT_TEXT_INSERTED;
    case blink::WebAXEventTextRemoved:
      return ui::AX_EVENT_TEXT_REMOVED;
    case blink::WebAXEventValueChanged:
      return ui::AX_EVENT_VALUE_CHANGED;
    default:
      // We can't add an assertion here, that prevents us
      // from adding new event enums in Blink.
      return ui::AX_EVENT_NONE;
  }
}

ui::AXTextDirection AXTextDirectionFromBlink(
    blink::WebAXTextDirection text_direction) {
  switch (text_direction) {
    case blink::WebAXTextDirectionLR:
      return ui::AX_TEXT_DIRECTION_LR;
    case blink::WebAXTextDirectionRL:
      return ui::AX_TEXT_DIRECTION_RL;
    case blink::WebAXTextDirectionTB:
      return ui::AX_TEXT_DIRECTION_TB;
    case blink::WebAXTextDirectionBT:
      return ui::AX_TEXT_DIRECTION_BT;
    default:
      NOTREACHED();
  }

  return ui::AX_TEXT_DIRECTION_NONE;
}

}  // namespace content
