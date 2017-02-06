// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>
#include <UIAutomationCore.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
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
    COM_INTERFACE_ENTRY(IAccessibleAction)
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
  CONTENT_EXPORT static const base::char16 kEmbeddedCharacter;

  // Mappings from roles and states to human readable strings. Initialize
  // with |InitializeStringMaps|.
  static std::map<int32_t, base::string16> role_string_map;
  static std::map<int32_t, base::string16> state_string_map;

  CONTENT_EXPORT BrowserAccessibilityWin();

  CONTENT_EXPORT ~BrowserAccessibilityWin() override;

  // Called after an atomic tree update completes. See
  // BrowserAccessibilityManagerWin::OnAtomicUpdateFinished for more
  // details on what these do.
  CONTENT_EXPORT void UpdateStep1ComputeWinAttributes();
  CONTENT_EXPORT void UpdateStep2ComputeHypertext();
  CONTENT_EXPORT void UpdateStep3FireEvents(bool is_subtree_creation);

  // This is used to call UpdateStep1ComputeWinAttributes, ... above when
  // a node needs to be updated for some other reason other than via
  // OnAtomicUpdateFinished.
  CONTENT_EXPORT void UpdatePlatformAttributes() override;

  //
  // BrowserAccessibility methods.
  //
  CONTENT_EXPORT void OnSubtreeWillBeDeleted() override;
  CONTENT_EXPORT void NativeAddReference() override;
  CONTENT_EXPORT void NativeReleaseReference() override;
  CONTENT_EXPORT bool IsNative() const override;
  CONTENT_EXPORT void OnLocationChanged() override;

  //
  // IAccessible methods.
  //

  // Performs the default action on a given object.
  CONTENT_EXPORT STDMETHODIMP accDoDefaultAction(VARIANT var_id) override;

  // Retrieves the child element or child object at a given point on the screen.
  CONTENT_EXPORT STDMETHODIMP
  accHitTest(LONG x_left, LONG y_top, VARIANT* child) override;

  // Retrieves the specified object's current screen location.
  CONTENT_EXPORT STDMETHODIMP accLocation(LONG* x_left,
                                          LONG* y_top,
                                          LONG* width,
                                          LONG* height,
                                          VARIANT var_id) override;

  // Traverses to another UI element and retrieves the object.
  CONTENT_EXPORT STDMETHODIMP
  accNavigate(LONG nav_dir, VARIANT start, VARIANT* end) override;

  // Retrieves an IDispatch interface pointer for the specified child.
  CONTENT_EXPORT STDMETHODIMP
  get_accChild(VARIANT var_child, IDispatch** disp_child) override;

  // Retrieves the number of accessible children.
  CONTENT_EXPORT STDMETHODIMP get_accChildCount(LONG* child_count) override;

  // Retrieves a string that describes the object's default action.
  CONTENT_EXPORT STDMETHODIMP
  get_accDefaultAction(VARIANT var_id, BSTR* default_action) override;

  // Retrieves the object's description.
  CONTENT_EXPORT STDMETHODIMP
  get_accDescription(VARIANT var_id, BSTR* desc) override;

  // Retrieves the object that has the keyboard focus.
  CONTENT_EXPORT STDMETHODIMP get_accFocus(VARIANT* focus_child) override;

  // Retrieves the help information associated with the object.
  CONTENT_EXPORT STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* heflp) override;

  // Retrieves the specified object's shortcut.
  CONTENT_EXPORT STDMETHODIMP
  get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key) override;

  // Retrieves the name of the specified object.
  CONTENT_EXPORT STDMETHODIMP get_accName(VARIANT var_id, BSTR* name) override;

  // Retrieves the IDispatch interface of the object's parent.
  CONTENT_EXPORT STDMETHODIMP get_accParent(IDispatch** disp_parent) override;

  // Retrieves information describing the role of the specified object.
  CONTENT_EXPORT STDMETHODIMP
  get_accRole(VARIANT var_id, VARIANT* role) override;

  // Retrieves the current state of the specified object.
  CONTENT_EXPORT STDMETHODIMP
  get_accState(VARIANT var_id, VARIANT* state) override;

  // Returns the value associated with the object.
  CONTENT_EXPORT STDMETHODIMP
  get_accValue(VARIANT var_id, BSTR* value) override;

  // Make an object take focus or extend the selection.
  CONTENT_EXPORT STDMETHODIMP
  accSelect(LONG flags_sel, VARIANT var_id) override;

  CONTENT_EXPORT STDMETHODIMP
  get_accHelpTopic(BSTR* help_file, VARIANT var_id, LONG* topic_id) override;

  CONTENT_EXPORT STDMETHODIMP get_accSelection(VARIANT* selected) override;

  // Deprecated methods, not implemented.
  CONTENT_EXPORT STDMETHODIMP
  put_accName(VARIANT var_id, BSTR put_name) override;
  CONTENT_EXPORT STDMETHODIMP
  put_accValue(VARIANT var_id, BSTR put_val) override;

  //
  // IAccessible2 methods.
  //

  // Returns role from a longer list of possible roles.
  CONTENT_EXPORT STDMETHODIMP role(LONG* role) override;

  // Returns the state bitmask from a larger set of possible states.
  CONTENT_EXPORT STDMETHODIMP get_states(AccessibleStates* states) override;

  // Returns the attributes specific to this IAccessible2 object,
  // such as a cell's formula.
  CONTENT_EXPORT STDMETHODIMP get_attributes(BSTR* attributes) override;

  // Get the unique ID of this object so that the client knows if it's
  // been encountered previously.
  CONTENT_EXPORT STDMETHODIMP get_uniqueID(LONG* unique_id) override;

  // Get the window handle of the enclosing window.
  CONTENT_EXPORT STDMETHODIMP get_windowHandle(HWND* window_handle) override;

  // Get this object's index in its parent object.
  CONTENT_EXPORT STDMETHODIMP get_indexInParent(LONG* index_in_parent) override;

  CONTENT_EXPORT STDMETHODIMP get_nRelations(LONG* n_relations) override;

  CONTENT_EXPORT STDMETHODIMP
  get_relation(LONG relation_index, IAccessibleRelation** relation) override;

  CONTENT_EXPORT STDMETHODIMP get_relations(LONG max_relations,
                                            IAccessibleRelation** relations,
                                            LONG* n_relations) override;

  CONTENT_EXPORT STDMETHODIMP scrollTo(enum IA2ScrollType scroll_type) override;

  CONTENT_EXPORT STDMETHODIMP
  scrollToPoint(enum IA2CoordinateType coordinate_type,
                LONG x,
                LONG y) override;

  CONTENT_EXPORT STDMETHODIMP
  get_groupPosition(LONG* group_level,
                    LONG* similar_items_in_group,
                    LONG* position_in_group) override;

  //
  // IAccessibleEx methods not implemented.
  //
  CONTENT_EXPORT STDMETHODIMP get_extendedRole(BSTR* extended_role) override;
  CONTENT_EXPORT STDMETHODIMP
  get_localizedExtendedRole(BSTR* localized_extended_role) override;
  CONTENT_EXPORT STDMETHODIMP
  get_nExtendedStates(LONG* n_extended_states) override;
  CONTENT_EXPORT STDMETHODIMP
  get_extendedStates(LONG max_extended_states,
                     BSTR** extended_states,
                     LONG* n_extended_states) override;
  CONTENT_EXPORT STDMETHODIMP
  get_localizedExtendedStates(LONG max_localized_extended_states,
                              BSTR** localized_extended_states,
                              LONG* n_localized_extended_states) override;
  CONTENT_EXPORT STDMETHODIMP get_locale(IA2Locale* locale) override;

  //
  // IAccessibleApplication methods.
  //
  CONTENT_EXPORT STDMETHODIMP get_appName(BSTR* app_name) override;

  CONTENT_EXPORT STDMETHODIMP get_appVersion(BSTR* app_version) override;

  CONTENT_EXPORT STDMETHODIMP get_toolkitName(BSTR* toolkit_name) override;

  CONTENT_EXPORT STDMETHODIMP
  get_toolkitVersion(BSTR* toolkit_version) override;

  //
  // IAccessibleImage methods.
  //
  CONTENT_EXPORT STDMETHODIMP get_description(BSTR* description) override;

  CONTENT_EXPORT STDMETHODIMP
  get_imagePosition(enum IA2CoordinateType coordinate_type,
                    LONG* x,
                    LONG* y) override;

  CONTENT_EXPORT STDMETHODIMP get_imageSize(LONG* height, LONG* width) override;

  //
  // IAccessibleTable methods.
  //

  // get_description - also used by IAccessibleImage

  CONTENT_EXPORT STDMETHODIMP
  get_accessibleAt(long row, long column, IUnknown** accessible) override;

  CONTENT_EXPORT STDMETHODIMP get_caption(IUnknown** accessible) override;

  CONTENT_EXPORT STDMETHODIMP
  get_childIndex(long row_index, long column_index, long* cell_index) override;

  CONTENT_EXPORT STDMETHODIMP
  get_columnDescription(long column, BSTR* description) override;

  CONTENT_EXPORT STDMETHODIMP
  get_columnExtentAt(long row, long column, long* n_columns_spanned) override;

  CONTENT_EXPORT STDMETHODIMP
  get_columnHeader(IAccessibleTable** accessible_table,
                   long* starting_row_index) override;

  CONTENT_EXPORT STDMETHODIMP
  get_columnIndex(long cell_index, long* column_index) override;

  CONTENT_EXPORT STDMETHODIMP get_nColumns(long* column_count) override;

  CONTENT_EXPORT STDMETHODIMP get_nRows(long* row_count) override;

  CONTENT_EXPORT STDMETHODIMP get_nSelectedChildren(long* cell_count) override;

  CONTENT_EXPORT STDMETHODIMP get_nSelectedColumns(long* column_count) override;

  CONTENT_EXPORT STDMETHODIMP get_nSelectedRows(long* row_count) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowDescription(long row, BSTR* description) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowExtentAt(long row, long column, long* n_rows_spanned) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowHeader(IAccessibleTable** accessible_table,
                long* starting_column_index) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowIndex(long cell_index, long* row_index) override;

  CONTENT_EXPORT STDMETHODIMP get_selectedChildren(long max_children,
                                                   long** children,
                                                   long* n_children) override;

  CONTENT_EXPORT STDMETHODIMP get_selectedColumns(long max_columns,
                                                  long** columns,
                                                  long* n_columns) override;

  CONTENT_EXPORT STDMETHODIMP
  get_selectedRows(long max_rows, long** rows, long* n_rows) override;

  CONTENT_EXPORT STDMETHODIMP get_summary(IUnknown** accessible) override;

  CONTENT_EXPORT STDMETHODIMP
  get_isColumnSelected(long column, boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP
  get_isRowSelected(long row, boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP
  get_isSelected(long row, long column, boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowColumnExtentsAtIndex(long index,
                              long* row,
                              long* column,
                              long* row_extents,
                              long* column_extents,
                              boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP selectRow(long row) override;

  CONTENT_EXPORT STDMETHODIMP selectColumn(long column) override;

  CONTENT_EXPORT STDMETHODIMP unselectRow(long row) override;

  CONTENT_EXPORT STDMETHODIMP unselectColumn(long column) override;

  CONTENT_EXPORT STDMETHODIMP
  get_modelChange(IA2TableModelChange* model_change) override;

  //
  // IAccessibleTable2 methods.
  //
  // (Most of these are duplicates of IAccessibleTable methods, only the
  // unique ones are included here.)
  //

  CONTENT_EXPORT STDMETHODIMP
  get_cellAt(long row, long column, IUnknown** cell) override;

  CONTENT_EXPORT STDMETHODIMP get_nSelectedCells(long* cell_count) override;

  CONTENT_EXPORT STDMETHODIMP
  get_selectedCells(IUnknown*** cells, long* n_selected_cells) override;

  CONTENT_EXPORT STDMETHODIMP
  get_selectedColumns(long** columns, long* n_columns) override;

  CONTENT_EXPORT STDMETHODIMP
  get_selectedRows(long** rows, long* n_rows) override;

  //
  // IAccessibleTableCell methods.
  //

  CONTENT_EXPORT STDMETHODIMP
  get_columnExtent(long* n_columns_spanned) override;

  CONTENT_EXPORT STDMETHODIMP
  get_columnHeaderCells(IUnknown*** cell_accessibles,
                        long* n_column_header_cells) override;

  CONTENT_EXPORT STDMETHODIMP get_columnIndex(long* column_index) override;

  CONTENT_EXPORT STDMETHODIMP get_rowExtent(long* n_rows_spanned) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowHeaderCells(IUnknown*** cell_accessibles,
                     long* n_row_header_cells) override;

  CONTENT_EXPORT STDMETHODIMP get_rowIndex(long* row_index) override;

  CONTENT_EXPORT STDMETHODIMP get_isSelected(boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP
  get_rowColumnExtents(long* row,
                       long* column,
                       long* row_extents,
                       long* column_extents,
                       boolean* is_selected) override;

  CONTENT_EXPORT STDMETHODIMP get_table(IUnknown** table) override;

  //
  // IAccessibleText methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nCharacters(LONG* n_characters) override;

  CONTENT_EXPORT STDMETHODIMP get_caretOffset(LONG* offset) override;

  CONTENT_EXPORT STDMETHODIMP
  get_characterExtents(LONG offset,
                       enum IA2CoordinateType coord_type,
                       LONG* out_x,
                       LONG* out_y,
                       LONG* out_width,
                       LONG* out_height) override;

  CONTENT_EXPORT STDMETHODIMP get_nSelections(LONG* n_selections) override;

  CONTENT_EXPORT STDMETHODIMP get_selection(LONG selection_index,
                                            LONG* start_offset,
                                            LONG* end_offset) override;

  CONTENT_EXPORT STDMETHODIMP
  get_text(LONG start_offset, LONG end_offset, BSTR* text) override;

  CONTENT_EXPORT STDMETHODIMP
  get_textAtOffset(LONG offset,
                   enum IA2TextBoundaryType boundary_type,
                   LONG* start_offset,
                   LONG* end_offset,
                   BSTR* text) override;

  CONTENT_EXPORT STDMETHODIMP
  get_textBeforeOffset(LONG offset,
                       enum IA2TextBoundaryType boundary_type,
                       LONG* start_offset,
                       LONG* end_offset,
                       BSTR* text) override;

  CONTENT_EXPORT STDMETHODIMP
  get_textAfterOffset(LONG offset,
                      enum IA2TextBoundaryType boundary_type,
                      LONG* start_offset,
                      LONG* end_offset,
                      BSTR* text) override;

  CONTENT_EXPORT STDMETHODIMP get_newText(IA2TextSegment* new_text) override;

  CONTENT_EXPORT STDMETHODIMP get_oldText(IA2TextSegment* old_text) override;

  CONTENT_EXPORT STDMETHODIMP
  get_offsetAtPoint(LONG x,
                    LONG y,
                    enum IA2CoordinateType coord_type,
                    LONG* offset) override;

  CONTENT_EXPORT STDMETHODIMP
  scrollSubstringTo(LONG start_index,
                    LONG end_index,
                    enum IA2ScrollType scroll_type) override;

  CONTENT_EXPORT STDMETHODIMP
  scrollSubstringToPoint(LONG start_index,
                         LONG end_index,
                         enum IA2CoordinateType coordinate_type,
                         LONG x,
                         LONG y) override;

  CONTENT_EXPORT STDMETHODIMP
  addSelection(LONG start_offset, LONG end_offset) override;

  CONTENT_EXPORT STDMETHODIMP removeSelection(LONG selection_index) override;

  CONTENT_EXPORT STDMETHODIMP setCaretOffset(LONG offset) override;

  CONTENT_EXPORT STDMETHODIMP setSelection(LONG selection_index,
                                           LONG start_offset,
                                           LONG end_offset) override;

  // IAccessibleText methods not implemented.
  CONTENT_EXPORT STDMETHODIMP get_attributes(LONG offset,
                                             LONG* start_offset,
                                             LONG* end_offset,
                                             BSTR* text_attributes) override;

  //
  // IAccessibleHypertext methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nHyperlinks(long* hyperlink_count) override;

  CONTENT_EXPORT STDMETHODIMP
  get_hyperlink(long index, IAccessibleHyperlink** hyperlink) override;

  CONTENT_EXPORT STDMETHODIMP
  get_hyperlinkIndex(long char_index, long* hyperlink_index) override;

  // IAccessibleHyperlink methods.
  CONTENT_EXPORT STDMETHODIMP get_anchor(long index, VARIANT* anchor) override;
  CONTENT_EXPORT STDMETHODIMP get_anchorTarget(long index,
                                               VARIANT* anchor_target) override;
  CONTENT_EXPORT STDMETHODIMP get_startIndex(long* index) override;
  CONTENT_EXPORT STDMETHODIMP get_endIndex(long* index) override;
  // This method is deprecated in the IA2 Spec and so we don't implement it.
  CONTENT_EXPORT STDMETHODIMP get_valid(boolean* valid) override;

  // IAccessibleAction mostly not implemented.
  CONTENT_EXPORT STDMETHODIMP nActions(long* n_actions) override;
  CONTENT_EXPORT STDMETHODIMP doAction(long action_index) override;
  CONTENT_EXPORT STDMETHODIMP
  get_description(long action_index, BSTR* description) override;
  CONTENT_EXPORT STDMETHODIMP get_keyBinding(long action_index,
                                             long n_max_bindings,
                                             BSTR** key_bindings,
                                             long* n_bindings) override;
  CONTENT_EXPORT STDMETHODIMP get_name(long action_index, BSTR* name) override;
  CONTENT_EXPORT STDMETHODIMP
  get_localizedName(long action_index, BSTR* localized_name) override;

  //
  // IAccessibleValue methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_currentValue(VARIANT* value) override;

  CONTENT_EXPORT STDMETHODIMP get_minimumValue(VARIANT* value) override;

  CONTENT_EXPORT STDMETHODIMP get_maximumValue(VARIANT* value) override;

  CONTENT_EXPORT STDMETHODIMP setCurrentValue(VARIANT new_value) override;

  //
  // ISimpleDOMDocument methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_URL(BSTR* url) override;

  CONTENT_EXPORT STDMETHODIMP get_title(BSTR* title) override;

  CONTENT_EXPORT STDMETHODIMP get_mimeType(BSTR* mime_type) override;

  CONTENT_EXPORT STDMETHODIMP get_docType(BSTR* doc_type) override;

  CONTENT_EXPORT STDMETHODIMP
  get_nameSpaceURIForID(short name_space_id, BSTR* name_space_uri) override;
  CONTENT_EXPORT STDMETHODIMP
  put_alternateViewMediaTypes(BSTR* comma_separated_media_types) override;

  //
  // ISimpleDOMNode methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_nodeInfo(BSTR* node_name,
                                           short* name_space_id,
                                           BSTR* node_value,
                                           unsigned int* num_children,
                                           unsigned int* unique_id,
                                           unsigned short* node_type) override;

  CONTENT_EXPORT STDMETHODIMP
  get_attributes(unsigned short max_attribs,
                 BSTR* attrib_names,
                 short* name_space_id,
                 BSTR* attrib_values,
                 unsigned short* num_attribs) override;

  CONTENT_EXPORT STDMETHODIMP
  get_attributesForNames(unsigned short num_attribs,
                         BSTR* attrib_names,
                         short* name_space_id,
                         BSTR* attrib_values) override;

  CONTENT_EXPORT STDMETHODIMP
  get_computedStyle(unsigned short max_style_properties,
                    boolean use_alternate_view,
                    BSTR* style_properties,
                    BSTR* style_values,
                    unsigned short* num_style_properties) override;

  CONTENT_EXPORT STDMETHODIMP
  get_computedStyleForProperties(unsigned short num_style_properties,
                                 boolean use_alternate_view,
                                 BSTR* style_properties,
                                 BSTR* style_values) override;

  CONTENT_EXPORT STDMETHODIMP scrollTo(boolean placeTopLeft) override;

  CONTENT_EXPORT STDMETHODIMP get_parentNode(ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP get_firstChild(ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP get_lastChild(ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP
  get_previousSibling(ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP get_nextSibling(ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP
  get_childAt(unsigned int child_index, ISimpleDOMNode** node) override;

  CONTENT_EXPORT STDMETHODIMP get_innerHTML(BSTR* innerHTML) override;

  CONTENT_EXPORT STDMETHODIMP
  get_localInterface(void** local_interface) override;

  CONTENT_EXPORT STDMETHODIMP get_language(BSTR* language) override;

  //
  // ISimpleDOMText methods.
  //

  CONTENT_EXPORT STDMETHODIMP get_domText(BSTR* dom_text) override;

  CONTENT_EXPORT STDMETHODIMP
  get_clippedSubstringBounds(unsigned int start_index,
                             unsigned int end_index,
                             int* out_x,
                             int* out_y,
                             int* out_width,
                             int* out_height) override;

  CONTENT_EXPORT STDMETHODIMP
  get_unclippedSubstringBounds(unsigned int start_index,
                               unsigned int end_index,
                               int* out_x,
                               int* out_y,
                               int* out_width,
                               int* out_height) override;

  CONTENT_EXPORT STDMETHODIMP
  scrollToSubstring(unsigned int start_index, unsigned int end_index) override;

  CONTENT_EXPORT STDMETHODIMP get_fontFamily(BSTR* font_family) override;

  //
  // IServiceProvider methods.
  //

  CONTENT_EXPORT STDMETHODIMP
  QueryService(REFGUID guidService, REFIID riid, void** object) override;

  // IAccessibleEx methods not implemented.
  CONTENT_EXPORT STDMETHODIMP
  GetObjectForChild(long child_id, IAccessibleEx** ret) override;

  CONTENT_EXPORT STDMETHODIMP
  GetIAccessiblePair(IAccessible** acc, long* child_id) override;

  CONTENT_EXPORT STDMETHODIMP GetRuntimeId(SAFEARRAY** runtime_id) override;

  CONTENT_EXPORT STDMETHODIMP
  ConvertReturnedElement(IRawElementProviderSimple* element,
                         IAccessibleEx** acc) override;

  //
  // IRawElementProviderSimple methods.
  //
  // The GetPatternProvider/GetPropertyValue methods need to be implemented for
  // the on-screen keyboard to show up in Windows 8 metro.
  CONTENT_EXPORT STDMETHODIMP
  GetPatternProvider(PATTERNID id, IUnknown** provider) override;
  CONTENT_EXPORT STDMETHODIMP
  GetPropertyValue(PROPERTYID id, VARIANT* ret) override;

  //
  // IRawElementProviderSimple methods not implemented
  //
  CONTENT_EXPORT STDMETHODIMP
  get_ProviderOptions(enum ProviderOptions* ret) override;
  CONTENT_EXPORT STDMETHODIMP
  get_HostRawElementProvider(IRawElementProviderSimple** provider) override;

  //
  // CComObjectRootEx methods.
  //

  // Called by BEGIN_COM_MAP() / END_COM_MAP().
  static CONTENT_EXPORT HRESULT WINAPI
  InternalQueryInterface(void* this_ptr,
                         const _ATL_INTMAP_ENTRY* entries,
                         REFIID iid,
                         void** object);

  // Computes and caches the IA2 text style attributes for the text and other
  // embedded child objects.
  CONTENT_EXPORT void ComputeStylesIfNeeded();

  CONTENT_EXPORT base::string16 GetText() const override;

  // Accessors.
  int32_t ia_role() const { return win_attributes_->ia_role; }
  int32_t ia_state() const { return win_attributes_->ia_state; }
  const base::string16& role_name() const { return win_attributes_->role_name; }
  int32_t ia2_role() const { return win_attributes_->ia2_role; }
  int32_t ia2_state() const { return win_attributes_->ia2_state; }
  const std::vector<base::string16>& ia2_attributes() const {
    return win_attributes_->ia2_attributes;
  }
  base::string16 name() const { return win_attributes_->name; }
  base::string16 description() const { return win_attributes_->description; }
  base::string16 value() const { return win_attributes_->value; }
  const std::map<int, std::vector<base::string16>>& offset_to_text_attributes()
      const {
    return win_attributes_->offset_to_text_attributes;
  }
  std::map<int32_t, int32_t>& hyperlink_offset_to_index() const {
    return win_attributes_->hyperlink_offset_to_index;
  }
  std::vector<int32_t>& hyperlinks() const {
    return win_attributes_->hyperlinks;
  }

 private:
  // Returns the IA2 text attributes for this object.
  std::vector<base::string16> ComputeTextAttributes() const;

  // Add one to the reference count and return the same object. Always
  // use this method when returning a BrowserAccessibilityWin object as
  // an output parameter to a COM interface, never use it otherwise.
  BrowserAccessibilityWin* NewReference();

  // Returns a list of IA2 attributes indicating the offsets in the text of a
  // leaf object, such as a text field or static text, where spelling errors are
  // present.
  std::map<int, std::vector<base::string16>> GetSpellingAttributes() const;

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

  // Escapes characters in string attributes as required by the IA2 Spec.
  // It's okay for input to be the same as output.
  CONTENT_EXPORT static void SanitizeStringAttributeForIA2(
      const base::string16& input,
      base::string16* output);
  FRIEND_TEST_ALL_PREFIXES(BrowserAccessibilityTest,
                           TestSanitizeStringAttributeForIA2);

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

  //
  // Helper methods for IA2 hyperlinks.
  //
  // Hyperlink is an IA2 misnomer. It refers to objects embedded within other
  // objects, such as a numbered list within a contenteditable div.
  // Also, in IA2, text that includes embedded objects is called hypertext.

  // Returns true if the current object is an IA2 hyperlink.
  bool IsHyperlink() const;
  // Returns the hyperlink at the given text position, or nullptr if no
  // hyperlink can be found.
  BrowserAccessibilityWin* GetHyperlinkFromHypertextOffset(int offset) const;

  // Functions for retrieving offsets for hyperlinks and hypertext.
  // Return -1 in case of failure.
  int32_t GetHyperlinkIndexFromChild(
      const BrowserAccessibilityWin& child) const;
  int32_t GetHypertextOffsetFromHyperlinkIndex(int32_t hyperlink_index) const;
  int32_t GetHypertextOffsetFromChild(
      const BrowserAccessibilityWin& child) const;
  int32_t GetHypertextOffsetFromDescendant(
      const BrowserAccessibilityWin& descendant) const;

  // If the selection endpoint is either equal to or an ancestor of this object,
  // returns endpoint_offset.
  // If the selection endpoint is a descendant of this object, returns its
  // offset. Otherwise, returns either 0 or the length of the hypertext
  // depending on the direction of the selection.
  // Returns -1 in case of unexpected failure, e.g. the selection endpoint
  // cannot be found in the accessibility tree.
  int GetHypertextOffsetFromEndpoint(
      const BrowserAccessibilityWin& endpoint_object,
      int endpoint_offset) const;

  //
  // Selection helper functions.
  //
  // The following functions retrieve the endpoints of the current selection.
  // First they check for a local selection found on the current control, e.g.
  // when querying the selection on a textarea.
  // If not found they retrieve the global selection found on the current frame.
  int GetSelectionAnchor() const;
  int GetSelectionFocus() const;
  // Retrieves the selection offsets in the way required by the IA2 APIs.
  // selection_start and selection_end are -1 when there is no selection active
  // on this object.
  // The greatest of the two offsets is one past the last character of the
  // selection.)
  void GetSelectionOffsets(int* selection_start, int* selection_end) const;

  // Get the value text, which might come from the floating-point
  // value for some roles.
  base::string16 GetValueText();

  bool IsSameHypertextCharacter(size_t old_char_index, size_t new_char_index);
  void ComputeHypertextRemovedAndInserted(
      int* start, int* old_len, int* new_len);

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

  // Searches forward from the given offset until the start of the next style
  // is found, or searches backward from the given offset until the start of the
  // current style is found.
  LONG FindStartOfStyle(LONG start_offset,
                        ui::TextBoundaryDirection direction) const;

  // ID refers to the node ID in the current tree, not the globally unique ID.
  // TODO(nektar): Could we use globally unique IDs everywhere?
  // TODO(nektar): Rename this function to GetFromNodeID.
  BrowserAccessibilityWin* GetFromID(int32_t id) const;

  // Returns true if this is a list box option with a parent of type list box,
  // or a menu list option with a parent of type menu list popup.
  bool IsListBoxOptionOrMenuListOption();

  // For adding / removing IA2 relations.

  void AddRelation(const base::string16& relation_type, int target_id);
  void AddBidirectionalRelations(const base::string16& relation_type,
                                 const base::string16& reverse_relation_type,
                                 ui::AXIntListAttribute attribute);
  // Clears all the forward relations from this object to any other object and
  // the associated  reverse relations on the other objects, but leaves any
  // reverse relations on this object alone.
  void ClearOwnRelations();
  void RemoveBidirectionalRelationsOfType(
      const base::string16& relation_type,
      const base::string16& reverse_relation_type);
  void RemoveTargetFromRelation(const base::string16& relation_type,
                                int target_id);

  // Updates object attributes of IA2 with html attributes.
  void UpdateRequiredAttributes();

  // Fire a Windows-specific accessibility event notification on this node.
  void FireNativeEvent(LONG win_event_type) const;

  struct WinAttributes {
    WinAttributes();
    ~WinAttributes();

    // IAccessible role and state.
    int32_t ia_role;
    int32_t ia_state;
    base::string16 role_name;

    // IAccessible name, description, help, value.
    base::string16 name;
    base::string16 description;
    base::string16 value;

    // IAccessible2 role and state.
    int32_t ia2_role;
    int32_t ia2_state;

    // IAccessible2 attributes.
    std::vector<base::string16> ia2_attributes;

    // Hypertext.
    base::string16 hypertext;

    // Maps each style span to its start offset in hypertext.
    std::map<int, std::vector<base::string16>> offset_to_text_attributes;

    // Maps the |hypertext_| embedded character offset to an index in
    // |hyperlinks_|.
    std::map<int32_t, int32_t> hyperlink_offset_to_index;

    // The unique id of a BrowserAccessibilityWin for each hyperlink.
    // TODO(nektar): Replace object IDs with child indices if we decide that
    // we are not implementing IA2 hyperlinks for anything other than IA2
    // Hypertext.
    std::vector<int32_t> hyperlinks;
  };

  std::unique_ptr<WinAttributes> win_attributes_;

  // Only valid during the scope of a IA2_EVENT_TEXT_REMOVED or
  // IA2_EVENT_TEXT_INSERTED event.
  std::unique_ptr<WinAttributes> old_win_attributes_;

  // Relationships between this node and other nodes.
  std::vector<BrowserAccessibilityRelation*> relations_;

  // The previous scroll position, so we can tell if this object scrolled.
  int previous_scroll_x_;
  int previous_scroll_y_;

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;
  friend class BrowserAccessibilityRelation;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityWin);
};

CONTENT_EXPORT BrowserAccessibilityWin*
ToBrowserAccessibilityWin(BrowserAccessibility* obj);

CONTENT_EXPORT const BrowserAccessibilityWin*
ToBrowserAccessibilityWin(const BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
