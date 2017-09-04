// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include "base/observer_list.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

// A simple interface for a class that wants to be notified when IAccessible2
// is used by a client, a strong indication that full accessibility support
// should be enabled.
class AX_EXPORT IAccessible2UsageObserver {
 public:
  IAccessible2UsageObserver();
  virtual ~IAccessible2UsageObserver();
  virtual void OnIAccessible2Used() = 0;
};

// Get an observer list that allows modules across the codebase to
// listen to when usage of IAccessible2 is detected.
extern AX_EXPORT base::ObserverList<IAccessible2UsageObserver>&
    GetIAccessible2UsageObserverList();

class __declspec(uuid("26f5641a-246d-457b-a96d-07f3fae6acf2"))
AXPlatformNodeWin
: public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible2_2, &IID_IAccessible2,
                         &LIBID_IAccessible2Lib>,
    public IAccessibleText,
    public IServiceProvider,
    public AXPlatformNodeBase {
 public:
  BEGIN_COM_MAP(AXPlatformNodeWin)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible2_2)
    COM_INTERFACE_ENTRY(AXPlatformNodeWin)
    COM_INTERFACE_ENTRY(IAccessible)
    COM_INTERFACE_ENTRY(IAccessible2)
    COM_INTERFACE_ENTRY(IAccessible2_2)
    COM_INTERFACE_ENTRY(IAccessibleText)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()

  ~AXPlatformNodeWin() override;

  // AXPlatformNode overrides.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // AXPlatformNodeBase overrides.
  void Destroy() override;
  int GetIndexInParent() override;

  //
  // IAccessible methods.
  //

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child) override;

  // Performs the object's default action.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id) override;

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id) override;

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end) override;

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child) override;

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count) override;

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id,
                                    BSTR* default_action) override;

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc) override;

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child) override;

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id,
                                       BSTR* access_key) override;

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name) override;

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent) override;

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role) override;

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state) override;

  // Gets the help string for the specified object.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help) override;

  // Retrieve or set the string value associated with the specified object.
  // Setting the value is not typically used by screen readers, but it's
  // used frequently by automation software.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value) override;
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR new_value) override;

  // IAccessible methods not implemented.
  STDMETHODIMP get_accSelection(VARIANT* selected) override;
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id) override;
  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id) override;
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) override;

  //
  // IAccessible2 methods.
  //

  STDMETHODIMP role(LONG* role) override;

  STDMETHODIMP get_states(AccessibleStates* states) override;

  STDMETHODIMP get_uniqueID(LONG* unique_id) override;

  STDMETHODIMP get_windowHandle(HWND* window_handle) override;

  STDMETHODIMP get_relationTargetsOfType(BSTR type,
                                         long max_targets,
                                         IUnknown*** targets,
                                         long* n_targets) override;

  STDMETHODIMP get_attributes(BSTR* attributes) override;

  STDMETHODIMP get_indexInParent(LONG* index_in_parent) override;

  //
  // IAccessible2 methods not implemented.
  //

  STDMETHODIMP get_attribute(BSTR name, VARIANT* attribute) override;
  STDMETHODIMP get_extendedRole(BSTR* extended_role) override;
  STDMETHODIMP get_nRelations(LONG* n_relations) override;
  STDMETHODIMP get_relation(LONG relation_index,
                            IAccessibleRelation** relation) override;
  STDMETHODIMP get_relations(LONG max_relations,
                             IAccessibleRelation** relations,
                             LONG* n_relations) override;
  STDMETHODIMP scrollTo(enum IA2ScrollType scroll_type) override;
  STDMETHODIMP scrollToPoint(enum IA2CoordinateType coordinate_type,
                             LONG x,
                             LONG y) override;
  STDMETHODIMP get_groupPosition(LONG* group_level,
                                 LONG* similar_items_in_group,
                                 LONG* position_in_group) override;
  STDMETHODIMP get_localizedExtendedRole(
      BSTR* localized_extended_role) override;
  STDMETHODIMP get_nExtendedStates(LONG* n_extended_states) override;
  STDMETHODIMP get_extendedStates(LONG max_extended_states,
                                  BSTR** extended_states,
                                  LONG* n_extended_states) override;
  STDMETHODIMP get_localizedExtendedStates(
      LONG max_localized_extended_states,
      BSTR** localized_extended_states,
      LONG* n_localized_extended_states) override;
  STDMETHODIMP get_locale(IA2Locale* locale) override;
  STDMETHODIMP get_accessibleWithCaret(IUnknown** accessible,
                                       long* caret_offset) override;

  //
  // IAccessibleText methods.
  //

  STDMETHODIMP get_nCharacters(LONG* n_characters) override;

  STDMETHODIMP get_caretOffset(LONG* offset) override;

  STDMETHODIMP get_nSelections(LONG* n_selections) override;

  STDMETHODIMP get_selection(LONG selection_index,
                             LONG* start_offset,
                             LONG* end_offset) override;

  STDMETHODIMP get_text(LONG start_offset,
                        LONG end_offset,
                        BSTR* text) override;

  STDMETHODIMP get_textAtOffset(LONG offset,
                                enum IA2TextBoundaryType boundary_type,
                                LONG* start_offset,
                                LONG* end_offset,
                                BSTR* text) override;

  STDMETHODIMP get_textBeforeOffset(LONG offset,
                                    enum IA2TextBoundaryType boundary_type,
                                    LONG* start_offset,
                                    LONG* end_offset,
                                    BSTR* text) override;

  STDMETHODIMP get_textAfterOffset(LONG offset,
                                   enum IA2TextBoundaryType boundary_type,
                                   LONG* start_offset,
                                   LONG* end_offset,
                                   BSTR* text) override;

  STDMETHODIMP get_offsetAtPoint(LONG x,
                                 LONG y,
                                 enum IA2CoordinateType coord_type,
                                 LONG* offset) override;

  //
  // IAccessibleText methods not implemented.
  //

  STDMETHODIMP get_newText(IA2TextSegment* new_text) override;
  STDMETHODIMP get_oldText(IA2TextSegment* old_text) override;
  STDMETHODIMP addSelection(LONG start_offset, LONG end_offset) override;
  STDMETHODIMP get_attributes(LONG offset,
                              LONG* start_offset,
                              LONG* end_offset,
                              BSTR* text_attributes) override;
  STDMETHODIMP get_characterExtents(LONG offset,
                                    enum IA2CoordinateType coord_type,
                                    LONG* x,
                                    LONG* y,
                                    LONG* width,
                                    LONG* height) override;
  STDMETHODIMP removeSelection(LONG selection_index) override;
  STDMETHODIMP setCaretOffset(LONG offset) override;
  STDMETHODIMP setSelection(LONG selection_index,
                            LONG start_offset,
                            LONG end_offset) override;
  STDMETHODIMP scrollSubstringTo(LONG start_index,
                                 LONG end_index,
                                 enum IA2ScrollType scroll_type) override;
  STDMETHODIMP scrollSubstringToPoint(LONG start_index,
                                      LONG end_index,
                                      enum IA2CoordinateType coordinate_type,
                                      LONG x,
                                      LONG y) override;

  //
  // IServiceProvider methods.
  //

  STDMETHODIMP QueryService(REFGUID guidService,
                            REFIID riid,
                            void** object) override;

 protected:
  AXPlatformNodeWin();

  // AXPlatformNodeBase overrides.
  void Dispose() override;

 private:
  bool IsValidId(const VARIANT& child) const;
  int MSAARole();
  int MSAAState();
  int MSAAEvent(ui::AXEvent event);

  HRESULT GetStringAttributeAsBstr(
      ui::AXStringAttribute attribute,
      BSTR* value_bstr) const;

  void AddAlertTarget();
  void RemoveAlertTarget();

  // Return the text to use for IAccessibleText.
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
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_H_
