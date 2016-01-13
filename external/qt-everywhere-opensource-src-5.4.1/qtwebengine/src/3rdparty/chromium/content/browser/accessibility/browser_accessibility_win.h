// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>
#include <UIAutomationCore.h>

#include <vector>

#include "base/compiler_specific.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMDocument.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "third_party/isimpledom/ISimpleDOMText.h"

namespace ui {
enum TextBoundaryDirection;
enum TextBoundaryType;
}

namespace content {
class BrowserAccessibilityRelation;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibilityWin
//
// Class implementing the windows accessible interface for the Browser-Renderer
// communication of accessibility information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class __declspec(uuid("562072fe-3390-43b1-9e2c-dd4118f5ac79"))
BrowserAccessibilityWin
    : public BrowserAccessibility,
      public CComObjectRootEx<CComMultiThreadModel>,
      public IDispatchImpl<IAccessible2, &IID_IAccessible2,
                           &LIBID_IAccessible2Lib>,
      public IAccessibleApplication,
      public IAccessibleHyperlink,
      public IAccessibleHypertext,
      public IAccessibleImage,
      public IAccessibleTable,
      public IAccessibleTable2,
      public IAccessibleTableCell,
      public IAccessibleValue,
      public IServiceProvider,
      public ISimpleDOMDocument,
      public ISimpleDOMNode,
      public ISimpleDOMText,
      public IAccessibleEx,
      public IRawElementProviderSimple {
 public:
  BEGIN_COM_MAP(BrowserAccessibilityWin)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible2)
    COM_INTERFACE_ENTRY(IAccessible)
    COM_INTERFACE_ENTRY(IAccessible2)
    COM_INTERFACE_ENTRY(IAccessibleApplication)
    COM_INTERFACE_ENTRY(IAccessibleEx)
    COM_INTERFACE_ENTRY(IAccessibleHyperlink)
    COM_INTERFACE_ENTRY(IAccessibleHypertext)
    COM_INTERFACE_ENTRY(IAccessibleImage)
    COM_INTERFACE_ENTRY(IAccessibleTable)
    COM_INTERFACE_ENTRY(IAccessibleTable2)
    COM_INTERFACE_ENTRY(IAccessibleTableCell)
    COM_INTERFACE_ENTRY(IAccessibleText)
    COM_INTERFACE_ENTRY(IAccessibleValue)
    COM_INTERFACE_ENTRY(IRawElementProviderSimple)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(ISimpleDOMDocument)
    COM_INTERFACE_ENTRY(ISimpleDOMNode)
    COM_INTERFACE_ENTRY(ISimpleDOMText)
  END_COM_MAP()

  // Represents a non-static text node in IAccessibleHypertext. This character
  // is embedded in the response to IAccessibleText::get_text, indicating the
  // position where a non-static text child object appears.
  CONTENT_EXPORT static const base::char16 kEmbeddedCharacter[];

  // Mappings from roles and states to human readable strings. Initialize
  // with |InitializeStringMaps|.
  static std::map<int32, base::string16> role_string_map;
  static std::map<int32, base::string16> state_string_map;

  CONTENT_EXPORT BrowserAccessibilityWin();

  CONTENT_EXPORT virtual ~BrowserAccessibilityWin();

  // The Windows-specific unique ID, used as the child ID for MSAA methods
  // like NotifyWinEvent, and as the unique ID for IAccessible2 and ISimpleDOM.
  LONG unique_id_win() const { return unique_id_win_; }

  //
  // BrowserAccessibility methods.
  //
  CONTENT_EXPORT virtual void OnDataChanged() OVERRIDE;
  CONTENT_EXPORT virtual void OnUpdateFinished() OVERRIDE;
  CONTENT_EXPORT virtual void NativeAddReference() OVERRIDE;
  CONTENT_EXPORT virtual void NativeReleaseReference() OVERRIDE;
  CONTENT_EXPORT virtual bool IsNative() const OVERRIDE;
  CONTENT_EXPORT virtual void OnLocationChanged() const OVERRIDE;

  //
  // IAccessible methods.
  //

  // Performs the default action on a given object.
  CONTENT_EXPORT STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // Retrieves the child element or child object at a given point on the screen.
  CONTENT_EXPORT STDMETHODIMP accHitTest(LONG x_left, LONG y_top,
                                         VARIANT* child);

  // Retrieves the specified object's current screen location.
  CONTENT_EXPORT STDMETHODIMP accLocation(LONG* x_left,
                                          LONG* y_top,
                                          LONG* width,
                                          LONG* height,
                                          VARIANT var_id);

  // Traverses to another UI element and retrieves the object.
  CONTENT_EXPORT STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start,
                                          VARIANT* end);

  // Retrieves an IDispatch interface pointer for the specified child.
  CONTENT_EXPORT STDMETHODIMP get_accChild(VARIANT var_child,
                                           IDispatch** disp_child);

  // Retrieves the number of accessible children.
  CONTENT_EXPORT STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves a string that describes the object's default action.
  CONTENT_EXPORT STDMETHODIMP get_accDefaultAction(VARIANT var_id,
                                                   BSTR* default_action);

  // Retrieves the object's description.
  CONTENT_EXPORT STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Retrieves the object that has the keyboard focus.
  CONTENT_EXPORT STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the help information associated with the object.
  CONTENT_EXPORT STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* heflp);

  // Retrieves the specified object's shortcut.
  CONTENT_EXPORT STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id,
                                                      BSTR* access_key);

  // Retrieves the name of the specified object.
  CONTENT_EXPORT STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the IDispatch interface of the object's parent.
  CONTENT_EXPORT STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Retrieves information describing the role of the specified object.
  CONTENT_EXPORT STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves the current state of the specified object.
  CONTENT_EXPORT STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Returns the value associated with the object.
  CONTENT_EXPORT STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Make an object take focus or extend the selection.
  CONTENT_EXPORT STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id);

  CONTENT_EXPORT STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                               VARIANT var_id,
                                               LONG* topic_id);

  CONTENT_EXPORT STDMETHODIMP get_accSelection(VARIANT* selected);

  // Deprecated methods, not implemented.
  CONTENT_EXPORT STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val) {
    return E_NOTIMPL;
  }

  //
  // IAccessible2 methods.
  //

  // Returns role from a longer list of possible roles.
  CONTENT_EXPORT STDMETHODIMP role(LONG* role);

  // Returns the state bitmask from a larger set of possible states.
  CONTENT_EXPORT STDMETHODIMP get_states(AccessibleStates* states);

  // Returns the attributes specific to this IAccessible2 object,
  // such as a cell's formula.
  CONTENT_EXPORT STDMETHODIMP get_attributes(BSTR* attributes);

  // Get the unique ID of this object so that the client knows if it's
  // been encountered previously.
  CONTENT_EXPORT STDMETHODIMP get_uniqueID(LONG* unique_id);

  // Get the window handle of the enclosing window.
  CONTENT_EXPORT STDMETHODIMP get_windowHandle(HWND* window_handle);

  // Get this object's index in its parent object.
  CONTENT_EXPORT STDMETHODIMP get_indexInParent(LONG* index_in_parent);

  CONTENT_EXPORT STDMETHODIMP get_nRelations(LONG* n_relations);

  CONTENT_EXPORT STDMETHODIMP get_relation(LONG relation_index,
                                           IAccessibleRelation** relation);

  CONTENT_EXPORT STDMETHODIMP get_relations(LONG max_relations,
                                            IAccessibleRelation** relations,
                                            LONG* n_relations);

  CONTENT_EXPORT STDMETHODIMP scrollTo(enum IA2ScrollType scroll_type);

  CONTENT_EXPORT STDMETHODIMP scrollToPoint(
      enum IA2CoordinateType coordinate_type,
      LONG x,
      LONG y);

  CONTENT_EXPORT STDMETHODIMP get_groupPosition(LONG* group_level,
                                                LONG* similar_items_in_group,
                                                LONG* position_in_group);

  //
  // IAccessibleEx methods not implemented.
  //
  CONTENT_EXPORT STDMETHODIMP get_extendedRole(BSTR* extended_role) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_localizedExtendedRole(
      BSTR* localized_extended_role) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_nExtendedStates(LONG* n_extended_states) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_extendedStates(LONG max_extended_states,
                                                 BSTR** extended_states,
                                                 LONG* n_extended_states) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_localizedExtendedStates(
      LONG max_localized_extended_states,
      BSTR** localized_extended_states,
      LONG* n_localized_extended_states) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_locale(IA2Locale* locale) {
    return E_NOTIMPL;
  }

  //
  // IAccessibleApplication methods.
  //
  CONTENT_EXPORT STDMETHODIMP get_appName(BSTR* app_name);

  CONTENT_EXPORT STDMETHODIMP get_appVersion(BSTR* app_version);

  CONTENT_EXPORT STDMETHODIMP get_toolkitName(BSTR* toolkit_name);

  CONTENT_EXPORT STDMETHODIMP get_toolkitVersion(BSTR* toolkit_version);

  //
  // IAccessibleImage methods.
  //
  CONTENT_EXPORT STDMETHODIMP get_description(BSTR* description);

  CONTENT_EXPORT STDMETHODIMP get_imagePosition(
      enum IA2CoordinateType coordinate_type,
      LONG* x,
      LONG* y);

  CONTENT_EXPORT STDMETHODIMP get_imageSize(LONG* height, LONG* width);

  //
  // IAccessibleTable methods.
  //

  // get_description - also used by IAccessibleImage

  CONTENT_EXPORT STDMETHODIMP get_accessibleAt(long row,
                                               long column,
                                               IUnknown** accessible);

  CONTENT_EXPORT STDMETHODIMP get_caption(IUnknown** accessible);

  CONTENT_EXPORT STDMETHODIMP get_childIndex(long row_index,
                                             long column_index,
                                             long* cell_index);

  CONTENT_EXPORT STDMETHODIMP get_columnDescription(long column,
                                                    BSTR* description);

  CONTENT_EXPORT STDMETHODIMP get_columnExtentAt(long row,
                                                 long column,
                                                 long* n_columns_spanned);

  CONTENT_EXPORT STDMETHODIMP get_columnHeader(
      IAccessibleTable** accessible_table,
      long* starting_row_index);

  CONTENT_EXPORT STDMETHODIMP get_columnIndex(long cell_index,
                                              long* column_index);

  CONTENT_EXPORT STDMETHODIMP get_nColumns(long* column_count);

  CONTENT_EXPORT STDMETHODIMP get_nRows(long* row_count);

  CONTENT_EXPORT STDMETHODIMP get_nSelectedChildren(long* cell_count);

  CONTENT_EXPORT STDMETHODIMP get_nSelectedColumns(long* column_count);

  CONTENT_EXPORT STDMETHODIMP get_nSelectedRows(long *row_count);

  CONTENT_EXPORT STDMETHODIMP get_rowDescription(long row,
                                                 BSTR* description);

  CONTENT_EXPORT STDMETHODIMP get_rowExtentAt(long row,
                                              long column,
                                              long* n_rows_spanned);

  CONTENT_EXPORT STDMETHODIMP get_rowHeader(IAccessibleTable** accessible_table,
                                            long* starting_column_index);

  CONTENT_EXPORT STDMETHODIMP get_rowIndex(long cell_index,
                                           long* row_index);

  CONTENT_EXPORT STDMETHODIMP get_selectedChildren(long max_children,
                                                   long** children,
                                                   long* n_children);

  CONTENT_EXPORT STDMETHODIMP get_selectedColumns(long max_columns,
                                                  long** columns,
                                                  long* n_columns);

  CONTENT_EXPORT STDMETHODIMP get_selectedRows(long max_rows,
                                               long** rows,
                                               long* n_rows);

  CONTENT_EXPORT STDMETHODIMP get_summary(IUnknown** accessible);

  CONTENT_EXPORT STDMETHODIMP get_isColumnSelected(long column,
                                                   boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP get_isRowSelected(long row,
                                                boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP get_isSelected(long row,
                                             long column,
                                             boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP get_rowColumnExtentsAtIndex(long index,
                                                          long* row,
                                                          long* column,
                                                          long* row_extents,
                                                          long* column_extents,
                                                          boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP selectRow(long row) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP selectColumn(long column) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP unselectRow(long row) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP unselectColumn(long column) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP get_modelChange(
      IA2TableModelChange* model_change) {
    return E_NOTIMPL;
  }

  //
  // IAccessibleTable2 methods.
  //
  // (Most of these are duplicates of IAccessibleTable methods, only the
  // unique ones are included here.)
  //

  CONTENT_EXPORT STDMETHODIMP get_cellAt(long row,
                                         long column,
                                         IUnknown** cell);

  CONTENT_EXPORT STDMETHODIMP get_nSelectedCells(long* cell_count);

  CONTENT_EXPORT STDMETHODIMP get_selectedCells(IUnknown*** cells,
                                                long* n_selected_cells);

  CONTENT_EXPORT STDMETHODIMP get_selectedColumns(long** columns,
                                                  long* n_columns);

  CONTENT_EXPORT STDMETHODIMP get_selectedRows(long** rows,
                                               long* n_rows);

  //
  // IAccessibleTableCell methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_columnExtent(long* n_columns_spanned);

  CONTENT_EXPORT STDMETHODIMP get_columnHeaderCells(
      IUnknown*** cell_accessibles,
      long* n_column_header_cells);

  CONTENT_EXPORT STDMETHODIMP get_columnIndex(long* column_index);

  CONTENT_EXPORT STDMETHODIMP get_rowExtent(long* n_rows_spanned);

  CONTENT_EXPORT STDMETHODIMP get_rowHeaderCells(IUnknown*** cell_accessibles,
                                                 long* n_row_header_cells);

  CONTENT_EXPORT STDMETHODIMP get_rowIndex(long* row_index);

  CONTENT_EXPORT STDMETHODIMP get_isSelected(boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP get_rowColumnExtents(long* row,
                                                   long* column,
                                                   long* row_extents,
                                                   long* column_extents,
                                                   boolean* is_selected);

  CONTENT_EXPORT STDMETHODIMP get_table(IUnknown** table);

  //
  // IAccessibleText methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nCharacters(LONG* n_characters);

  CONTENT_EXPORT STDMETHODIMP get_caretOffset(LONG* offset);

  CONTENT_EXPORT STDMETHODIMP get_characterExtents(
      LONG offset,
      enum IA2CoordinateType coord_type,
      LONG* out_x,
      LONG* out_y,
      LONG* out_width,
      LONG* out_height);

  CONTENT_EXPORT STDMETHODIMP get_nSelections(LONG* n_selections);

  CONTENT_EXPORT STDMETHODIMP get_selection(LONG selection_index,
                                            LONG* start_offset,
                                            LONG* end_offset);

  CONTENT_EXPORT STDMETHODIMP get_text(LONG start_offset,
                                       LONG end_offset,
                                       BSTR* text);

  CONTENT_EXPORT STDMETHODIMP get_textAtOffset(
      LONG offset,
      enum IA2TextBoundaryType boundary_type,
      LONG* start_offset,
      LONG* end_offset,
      BSTR* text);

  CONTENT_EXPORT STDMETHODIMP get_textBeforeOffset(
      LONG offset,
      enum IA2TextBoundaryType boundary_type,
      LONG* start_offset,
      LONG* end_offset,
      BSTR* text);

  CONTENT_EXPORT STDMETHODIMP get_textAfterOffset(
      LONG offset,
      enum IA2TextBoundaryType boundary_type,
      LONG* start_offset,
      LONG* end_offset,
      BSTR* text);

  CONTENT_EXPORT STDMETHODIMP get_newText(IA2TextSegment* new_text);

  CONTENT_EXPORT STDMETHODIMP get_oldText(IA2TextSegment* old_text);

  CONTENT_EXPORT STDMETHODIMP get_offsetAtPoint(
      LONG x,
      LONG y,
      enum IA2CoordinateType coord_type,
      LONG* offset);

  CONTENT_EXPORT STDMETHODIMP scrollSubstringTo(
       LONG start_index,
       LONG end_index,
       enum IA2ScrollType scroll_type);

  CONTENT_EXPORT STDMETHODIMP scrollSubstringToPoint(
      LONG start_index,
      LONG end_index,
      enum IA2CoordinateType coordinate_type,
      LONG x, LONG y);

  CONTENT_EXPORT STDMETHODIMP addSelection(LONG start_offset, LONG end_offset);

  CONTENT_EXPORT STDMETHODIMP removeSelection(LONG selection_index);

  CONTENT_EXPORT STDMETHODIMP setCaretOffset(LONG offset);

  CONTENT_EXPORT STDMETHODIMP setSelection(LONG selection_index,
                                           LONG start_offset,
                                           LONG end_offset);

  // IAccessibleText methods not implemented.
  CONTENT_EXPORT STDMETHODIMP get_attributes(LONG offset, LONG* start_offset,
                                             LONG* end_offset,
                                             BSTR* text_attributes) {
    return E_NOTIMPL;
  }

  //
  // IAccessibleHypertext methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nHyperlinks(long* hyperlink_count);

  CONTENT_EXPORT STDMETHODIMP get_hyperlink(long index,
                                            IAccessibleHyperlink** hyperlink);

  CONTENT_EXPORT STDMETHODIMP get_hyperlinkIndex(long char_index,
                                                 long* hyperlink_index);

  // IAccessibleHyperlink not implemented.
  CONTENT_EXPORT STDMETHODIMP get_anchor(long index, VARIANT* anchor) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_anchorTarget(long index,
                                               VARIANT* anchor_target) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_startIndex( long* index) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_endIndex( long* index) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_valid(boolean* valid) {
    return E_NOTIMPL;
  }

  // IAccessibleAction not implemented.
  CONTENT_EXPORT STDMETHODIMP nActions(long* n_actions) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP doAction(long action_index) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_description(long action_index,
                                              BSTR* description) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_keyBinding(long action_index,
                                             long n_max_bindings,
                                             BSTR** key_bindings,
                                             long* n_bindings) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_name(long action_index, BSTR* name) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP get_localizedName(long action_index,
                                                BSTR* localized_name) {
    return E_NOTIMPL;
  }

  //
  // IAccessibleValue methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_currentValue(VARIANT* value);

  CONTENT_EXPORT STDMETHODIMP get_minimumValue(VARIANT* value);

  CONTENT_EXPORT STDMETHODIMP get_maximumValue(VARIANT* value);

  CONTENT_EXPORT STDMETHODIMP setCurrentValue(VARIANT new_value);

  //
  // ISimpleDOMDocument methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_URL(BSTR* url);

  CONTENT_EXPORT STDMETHODIMP get_title(BSTR* title);

  CONTENT_EXPORT STDMETHODIMP get_mimeType(BSTR* mime_type);

  CONTENT_EXPORT STDMETHODIMP get_docType(BSTR* doc_type);

  CONTENT_EXPORT STDMETHODIMP get_nameSpaceURIForID(short name_space_id,
                                                    BSTR* name_space_uri) {
    return E_NOTIMPL;
  }
  CONTENT_EXPORT STDMETHODIMP put_alternateViewMediaTypes(
      BSTR* comma_separated_media_types) {
    return E_NOTIMPL;
  }

  //
  // ISimpleDOMNode methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nodeInfo(BSTR* node_name,
                                           short* name_space_id,
                                           BSTR* node_value,
                                           unsigned int* num_children,
                                           unsigned int* unique_id,
                                           unsigned short* node_type);

  CONTENT_EXPORT STDMETHODIMP get_attributes(unsigned short max_attribs,
                                             BSTR* attrib_names,
                                             short* name_space_id,
                                             BSTR* attrib_values,
                                             unsigned short* num_attribs);

  CONTENT_EXPORT STDMETHODIMP get_attributesForNames(
      unsigned short num_attribs,
      BSTR* attrib_names,
      short* name_space_id,
      BSTR* attrib_values);

  CONTENT_EXPORT STDMETHODIMP get_computedStyle(
      unsigned short max_style_properties,
      boolean use_alternate_view,
      BSTR *style_properties,
      BSTR *style_values,
      unsigned short *num_style_properties);

  CONTENT_EXPORT STDMETHODIMP get_computedStyleForProperties(
      unsigned short num_style_properties,
      boolean use_alternate_view,
      BSTR* style_properties,
      BSTR* style_values);

  CONTENT_EXPORT STDMETHODIMP scrollTo(boolean placeTopLeft);

  CONTENT_EXPORT STDMETHODIMP get_parentNode(ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_firstChild(ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_lastChild(ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_previousSibling(ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_nextSibling(ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_childAt(unsigned int child_index,
                                          ISimpleDOMNode** node);

  CONTENT_EXPORT STDMETHODIMP get_innerHTML(BSTR* innerHTML) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP get_localInterface(void** local_interface) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP get_language(BSTR* language) {
    return E_NOTIMPL;
  }

  //
  // ISimpleDOMText methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_domText(BSTR* dom_text);

  CONTENT_EXPORT STDMETHODIMP get_clippedSubstringBounds(
      unsigned int start_index,
      unsigned int end_index,
      int* out_x,
      int* out_y,
      int* out_width,
      int* out_height);

  CONTENT_EXPORT STDMETHODIMP get_unclippedSubstringBounds(
      unsigned int start_index,
      unsigned int end_index,
      int* out_x,
      int* out_y,
      int* out_width,
      int* out_height);

  CONTENT_EXPORT STDMETHODIMP scrollToSubstring(unsigned int start_index,
                                                unsigned int end_index);

  CONTENT_EXPORT STDMETHODIMP get_fontFamily(BSTR *font_family)  {
    return E_NOTIMPL;
  }

  //
  // IServiceProvider methods.
  //

  CONTENT_EXPORT STDMETHODIMP QueryService(REFGUID guidService,
                                           REFIID riid,
                                           void** object);

  // IAccessibleEx methods not implemented.
  CONTENT_EXPORT STDMETHODIMP GetObjectForChild(long child_id,
                                                IAccessibleEx** ret) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP GetIAccessiblePair(IAccessible** acc,
                                                 long* child_id) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP GetRuntimeId(SAFEARRAY** runtime_id) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP ConvertReturnedElement(
      IRawElementProviderSimple* element,
      IAccessibleEx** acc) {
    return E_NOTIMPL;
  }

  //
  // IRawElementProviderSimple methods.
  //
  // The GetPatternProvider/GetPropertyValue methods need to be implemented for
  // the on-screen keyboard to show up in Windows 8 metro.
  CONTENT_EXPORT STDMETHODIMP GetPatternProvider(PATTERNID id,
                                                 IUnknown** provider);
  CONTENT_EXPORT STDMETHODIMP GetPropertyValue(PROPERTYID id, VARIANT* ret);

  //
  // IRawElementProviderSimple methods not implemented
  //
  CONTENT_EXPORT STDMETHODIMP get_ProviderOptions(enum ProviderOptions* ret) {
    return E_NOTIMPL;
  }

  CONTENT_EXPORT STDMETHODIMP get_HostRawElementProvider(
      IRawElementProviderSimple** provider) {
    return E_NOTIMPL;
  }

  //
  // CComObjectRootEx methods.
  //

  CONTENT_EXPORT HRESULT WINAPI InternalQueryInterface(
      void* this_ptr,
      const _ATL_INTMAP_ENTRY* entries,
      REFIID iid,
      void** object);

  // Accessors.
  int32 ia_role() const { return ia_role_; }
  int32 ia_state() const { return ia_state_; }
  const base::string16& role_name() const { return role_name_; }
  int32 ia2_role() const { return ia2_role_; }
  int32 ia2_state() const { return ia2_state_; }
  const std::vector<base::string16>& ia2_attributes() const {
    return ia2_attributes_;
  }

 private:
  // Add one to the reference count and return the same object. Always
  // use this method when returning a BrowserAccessibilityWin object as
  // an output parameter to a COM interface, never use it otherwise.
  BrowserAccessibilityWin* NewReference();

  // Many MSAA methods take a var_id parameter indicating that the operation
  // should be performed on a particular child ID, rather than this object.
  // This method tries to figure out the target object from |var_id| and
  // returns a pointer to the target object if it exists, otherwise NULL.
  // Does not return a new reference.
  BrowserAccessibilityWin* GetTargetFromChildID(const VARIANT& var_id);

  // Initialize the role and state metadata from the role enum and state
  // bitmasks defined in ui::AXNodeData.
  void InitRoleAndState();

  // Retrieve the value of an attribute from the string attribute map and
  // if found and nonempty, allocate a new BSTR (with SysAllocString)
  // and return S_OK. If not found or empty, return S_FALSE.
  HRESULT GetStringAttributeAsBstr(
      ui::AXStringAttribute attribute,
      BSTR* value_bstr);

  // If the string attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |ia2_attr|.
  void StringAttributeToIA2(ui::AXStringAttribute attribute,
                            const char* ia2_attr);

  // If the bool attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |ia2_attr|.
  void BoolAttributeToIA2(ui::AXBoolAttribute attribute,
                          const char* ia2_attr);

  // If the int attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |ia2_attr|.
  void IntAttributeToIA2(ui::AXIntAttribute attribute,
                         const char* ia2_attr);

  // Get the value text, which might come from the floating-point
  // value for some roles.
  base::string16 GetValueText();

  // Get the text of this node for the purposes of IAccessibleText - it may
  // be the name, it may be the value, etc. depending on the role.
  base::string16 TextForIAccessibleText();

  // If offset is a member of IA2TextSpecialOffsets this function updates the
  // value of offset and returns, otherwise offset remains unchanged.
  void HandleSpecialTextOffset(const base::string16& text, LONG* offset);

  // Convert from a IA2TextBoundaryType to a ui::TextBoundaryType.
  ui::TextBoundaryType IA2TextBoundaryToTextBoundary(IA2TextBoundaryType type);

  // Search forwards (direction == 1) or backwards (direction == -1)
  // from the given offset until the given boundary is found, and
  // return the offset of that boundary.
  LONG FindBoundary(const base::string16& text,
                    IA2TextBoundaryType ia2_boundary,
                    LONG start_offset,
                    ui::TextBoundaryDirection direction);

  // Return a pointer to the object corresponding to the given id,
  // does not make a new reference.
  BrowserAccessibilityWin* GetFromID(int32 id);

  // Windows-specific unique ID (unique within the browser process),
  // used for get_accChild, NotifyWinEvent, and as the unique ID for
  // IAccessible2 and ISimpleDOM.
  LONG unique_id_win_;

  // IAccessible role and state.
  int32 ia_role_;
  int32 ia_state_;
  base::string16 role_name_;

  // IAccessible2 role and state.
  int32 ia2_role_;
  int32 ia2_state_;

  // IAccessible2 attributes.
  std::vector<base::string16> ia2_attributes_;

  // True in Initialize when the object is first created, and false
  // subsequent times.
  bool first_time_;

  // The previous text, before the last update to this object.
  base::string16 previous_text_;

  // The old text to return in IAccessibleText::get_oldText - this is like
  // previous_text_ except that it's NOT updated when the object
  // is initialized again but the text doesn't change.
  base::string16 old_text_;

  // The previous state, used to see if there was a state change.
  int32 old_ia_state_;

  // Relationships between this node and other nodes.
  std::vector<BrowserAccessibilityRelation*> relations_;

  // The text of this node including embedded hyperlink characters.
  base::string16 hypertext_;

  // Maps the |hypertext_| embedded character offset to an index in
  // |hyperlinks_|.
  std::map<int32, int32> hyperlink_offset_to_index_;

  // Collection of non-static text child indicies, each of which corresponds to
  // a hyperlink.
  std::vector<int32> hyperlinks_;

  // The previous scroll position, so we can tell if this object scrolled.
  int previous_scroll_x_;
  int previous_scroll_y_;

  // The next unique id to use.
  static LONG next_unique_id_win_;

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;
  friend class BrowserAccessibilityRelation;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
