// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <limits.h>
#include <oleacc.h>
#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/base/win/atl_module.h"

//
// Macros to use at the top of any AXPlatformNodeWin function that implements
// a COM interface. Because COM objects are reference counted and clients
// are completely untrusted, it's important to always first check that our
// object is still valid, and then check that all pointer arguments are
// not NULL.
//

#define COM_OBJECT_VALIDATE() \
    if (!delegate_) \
      return E_FAIL;
#define COM_OBJECT_VALIDATE_1_ARG(arg) \
    if (!delegate_) return E_FAIL; \
    if (!arg) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_2_ARGS(arg1, arg2) \
    if (!delegate_) return E_FAIL; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_3_ARGS(arg1, arg2, arg3) \
    if (!delegate_) return E_FAIL; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG; \
    if (!arg3) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_4_ARGS(arg1, arg2, arg3, arg4) \
    if (!delegate_) return E_FAIL; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG; \
    if (!arg3) return E_INVALIDARG; \
    if (!arg4) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_VAR_ID(var_id) \
    if (!delegate_) return E_FAIL; \
    if (!IsValidId(var_id)) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, arg) \
    if (!delegate_) return E_FAIL; \
    if (!IsValidId(var_id)) return E_INVALIDARG; \
    if (!arg) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_VAR_ID_2_ARGS(var_id, arg1, arg2) \
    if (!delegate_) return E_FAIL; \
    if (!IsValidId(var_id)) return E_INVALIDARG; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_VAR_ID_3_ARGS(var_id, arg1, arg2, arg3) \
    if (!delegate_) return E_FAIL; \
    if (!IsValidId(var_id)) return E_INVALIDARG; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG; \
    if (!arg3) return E_INVALIDARG
#define COM_OBJECT_VALIDATE_VAR_ID_4_ARGS(var_id, arg1, arg2, arg3, arg4) \
    if (!delegate_) return E_FAIL; \
    if (!IsValidId(var_id)) return E_INVALIDARG; \
    if (!arg1) return E_INVALIDARG; \
    if (!arg2) return E_INVALIDARG; \
    if (!arg3) return E_INVALIDARG; \
    if (!arg4) return E_INVALIDARG

namespace ui {

namespace {

typedef base::hash_set<AXPlatformNodeWin*> AXPlatformNodeWinSet;
// Set of all AXPlatformNodeWin objects that were the target of an
// alert event.
base::LazyInstance<AXPlatformNodeWinSet> g_alert_targets =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<IAccessible2UsageObserver>>
    g_iaccessible2_usage_observer_list = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// IAccessible2UsageObserver
//

IAccessible2UsageObserver::IAccessible2UsageObserver() {
}

IAccessible2UsageObserver::~IAccessible2UsageObserver() {
}

// static
base::ObserverList<IAccessible2UsageObserver>&
    GetIAccessible2UsageObserverList() {
  return g_iaccessible2_usage_observer_list.Get();
}

//
// AXPlatformNode::Create
//

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  // Make sure ATL is initialized in this module.
  ui::win::CreateATLModuleIfNeeded();

  CComObject<AXPlatformNodeWin>* instance = nullptr;
  HRESULT hr = CComObject<AXPlatformNodeWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  instance->Init(delegate);
  instance->AddRef();
  return instance;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  base::win::ScopedComPtr<AXPlatformNodeWin> ax_platform_node;
  accessible->QueryInterface(ax_platform_node.Receive());
  return ax_platform_node.get();
}

//
// AXPlatformNodeWin
//

AXPlatformNodeWin::AXPlatformNodeWin() {
}

AXPlatformNodeWin::~AXPlatformNodeWin() {
}

//
// AXPlatformNodeBase implementation.
//

void AXPlatformNodeWin::Dispose() {
  Release();
}

void AXPlatformNodeWin::Destroy() {
  RemoveAlertTarget();
  AXPlatformNodeBase::Destroy();
}

//
// AXPlatformNode implementation.
//

gfx::NativeViewAccessible AXPlatformNodeWin::GetNativeViewAccessible() {
  return this;
}

void AXPlatformNodeWin::NotifyAccessibilityEvent(ui::AXEvent event_type) {
  HWND hwnd = delegate_->GetTargetForNativeAccessibilityEvent();
  if (!hwnd)
    return;

  // Menu items fire selection events but Windows screen readers work reliably
  // with focus events. Remap here.
  if (event_type == ui::AX_EVENT_SELECTION &&
      GetData().role == ui::AX_ROLE_MENU_ITEM)
    event_type = ui::AX_EVENT_FOCUS;

  int native_event = MSAAEvent(event_type);
  if (native_event < EVENT_MIN)
    return;

  ::NotifyWinEvent(native_event, hwnd, OBJID_CLIENT, -unique_id_);

  // Keep track of objects that are a target of an alert event.
  if (event_type == ui::AX_EVENT_ALERT)
    AddAlertTarget();
}

int AXPlatformNodeWin::GetIndexInParent() {
  base::win::ScopedComPtr<IDispatch> parent_dispatch;
  base::win::ScopedComPtr<IAccessible> parent_accessible;
  if (S_OK != get_accParent(parent_dispatch.Receive()))
    return -1;
  if (S_OK != parent_dispatch.QueryInterface(parent_accessible.Receive()))
    return -1;

  LONG child_count = 0;
  if (S_OK != parent_accessible->get_accChildCount(&child_count))
    return -1;
  for (LONG index = 1; index <= child_count; ++index) {
    base::win::ScopedVariant childid_index(index);
    base::win::ScopedComPtr<IDispatch> child_dispatch;
    base::win::ScopedComPtr<IAccessible> child_accessible;
    if (S_OK == parent_accessible->get_accChild(childid_index,
                                                child_dispatch.Receive()) &&
        S_OK == child_dispatch.QueryInterface(child_accessible.Receive())) {
      if (child_accessible.get() == this)
        return index - 1;
    }
  }

  return -1;
}

//
// IAccessible implementation.
//

STDMETHODIMP AXPlatformNodeWin::accHitTest(
    LONG x_left, LONG y_top, VARIANT* child) {
  COM_OBJECT_VALIDATE_1_ARG(child);
  gfx::NativeViewAccessible hit_child = delegate_->HitTestSync(x_left, y_top);
  if (!hit_child) {
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (hit_child == this) {
    // This object is the best match, so return CHILDID_SELF. It's tempting to
    // simplify the logic and use VT_DISPATCH everywhere, but the Windows
    // call AccessibleObjectFromPoint will keep calling accHitTest until some
    // object returns CHILDID_SELF.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
    return S_OK;
  }

  // Call accHitTest recursively on the result, which may be a recursive call
  // to this function or it may be overridden, for example in the case of a
  // WebView.
  HRESULT result = hit_child->accHitTest(x_left, y_top, child);

  // If the recursive call returned CHILDID_SELF, we have to convert that
  // into a VT_DISPATCH for the return value to this call.
  if (S_OK == result && child->vt == VT_I4 && child->lVal == CHILDID_SELF) {
    child->vt = VT_DISPATCH;
    child->pdispVal = hit_child;
    // Always increment ref when returning a reference to a COM object.
    child->pdispVal->AddRef();
  }
  return result;
}

HRESULT AXPlatformNodeWin::accDoDefaultAction(VARIANT var_id) {
  COM_OBJECT_VALIDATE_VAR_ID(var_id);
  delegate_->DoDefaultAction();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::accLocation(
    LONG* x_left, LONG* y_top, LONG* width, LONG* height, VARIANT var_id) {
  COM_OBJECT_VALIDATE_VAR_ID_4_ARGS(var_id, x_left, y_top, width, height);
  gfx::Rect bounds = GetData().location;
  bounds += delegate_->GetGlobalCoordinateOffset();
  *x_left = bounds.x();
  *y_top  = bounds.y();
  *width  = bounds.width();
  *height = bounds.height();

  if (bounds.IsEmpty())
    return S_FALSE;

  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::accNavigate(
    LONG nav_dir, VARIANT start, VARIANT* end) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(start, end);
  IAccessible* result = nullptr;

  switch (nav_dir) {
    case NAVDIR_DOWN:
    case NAVDIR_UP:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
      // These directions are not implemented, matching Mozilla and IE.
      return E_NOTIMPL;

    case NAVDIR_FIRSTCHILD:
      if (delegate_->GetChildCount() > 0)
        result = delegate_->ChildAtIndex(0);
      break;

    case NAVDIR_LASTCHILD:
      if (delegate_->GetChildCount() > 0)
        result = delegate_->ChildAtIndex(delegate_->GetChildCount() - 1);
      break;

    case NAVDIR_NEXT: {
      AXPlatformNodeBase* next = GetNextSibling();
      if (next)
        result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_PREVIOUS: {
      AXPlatformNodeBase* previous = GetPreviousSibling();
      if (previous)
        result = previous->GetNativeViewAccessible();
      break;
    }

    default:
      return E_INVALIDARG;
  }

  if (!result) {
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  end->vt = VT_DISPATCH;
  end->pdispVal = result;
  // Always increment ref when returning a reference to a COM object.
  end->pdispVal->AddRef();

  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_accChild(VARIANT var_child,
                                             IDispatch** disp_child) {
  COM_OBJECT_VALIDATE_1_ARG(disp_child);
  LONG child_id = V_I4(&var_child);
  if (child_id == CHILDID_SELF) {
    *disp_child = this;
    (*disp_child)->AddRef();
    return S_OK;
  }

  if (child_id >= 1 && child_id <= delegate_->GetChildCount()) {
    // Positive child ids are a 1-based child index, used by clients
    // that want to enumerate all immediate children.
    *disp_child = delegate_->ChildAtIndex(child_id - 1);
    if (!(*disp_child))
      return E_INVALIDARG;
    (*disp_child)->AddRef();
    return S_OK;
  }

  if (child_id >= 0)
    return E_INVALIDARG;

  // Negative child ids can be used to map to any descendant.
  AXPlatformNodeWin* child = static_cast<AXPlatformNodeWin*>(
      GetFromUniqueId(-child_id));
  if (child && !IsDescendant(child))
    child = nullptr;

  if (child) {
    *disp_child = child;
    (*disp_child)->AddRef();
    return S_OK;
  }

  *disp_child = nullptr;
  return E_INVALIDARG;
}

STDMETHODIMP AXPlatformNodeWin::get_accChildCount(LONG* child_count) {
  COM_OBJECT_VALIDATE_1_ARG(child_count);
  *child_count = delegate_->GetChildCount();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_accDefaultAction(
    VARIANT var_id, BSTR* def_action) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, def_action);
  return GetStringAttributeAsBstr(ui::AX_ATTR_ACTION, def_action);
}

STDMETHODIMP AXPlatformNodeWin::get_accDescription(
    VARIANT var_id, BSTR* desc) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, desc);
  return GetStringAttributeAsBstr(ui::AX_ATTR_DESCRIPTION, desc);
}

STDMETHODIMP AXPlatformNodeWin::get_accFocus(VARIANT* focus_child) {
  COM_OBJECT_VALIDATE_1_ARG(focus_child);
  gfx::NativeViewAccessible focus_accessible = delegate_->GetFocus();
  if (focus_accessible == this) {
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus_accessible) {
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus_accessible;
    focus_child->pdispVal->AddRef();
    return S_OK;
  } else {
    focus_child->vt = VT_EMPTY;
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_accKeyboardShortcut(
    VARIANT var_id, BSTR* acc_key) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, acc_key);
  return GetStringAttributeAsBstr(ui::AX_ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP AXPlatformNodeWin::get_accName(
    VARIANT var_id, BSTR* name) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, name);
  return GetStringAttributeAsBstr(ui::AX_ATTR_NAME, name);
}

STDMETHODIMP AXPlatformNodeWin::get_accParent(
    IDispatch** disp_parent) {
  COM_OBJECT_VALIDATE_1_ARG(disp_parent);
  *disp_parent = GetParent();
  if (*disp_parent) {
    (*disp_parent)->AddRef();
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP AXPlatformNodeWin::get_accRole(
    VARIANT var_id, VARIANT* role) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, role);
  role->vt = VT_I4;
  role->lVal = MSAARole();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_accState(
    VARIANT var_id, VARIANT* state) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, state);
  state->vt = VT_I4;
  state->lVal = MSAAState();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_accHelp(
    VARIANT var_id, BSTR* help) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, help);
  return S_FALSE;
}

STDMETHODIMP AXPlatformNodeWin::get_accValue(VARIANT var_id, BSTR* value) {
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG(var_id, value);
  return GetStringAttributeAsBstr(ui::AX_ATTR_VALUE, value);
}

STDMETHODIMP AXPlatformNodeWin::put_accValue(VARIANT var_id,
                                             BSTR new_value) {
  COM_OBJECT_VALIDATE_VAR_ID(var_id);
  if (delegate_->SetStringValue(new_value))
    return S_OK;
  return E_FAIL;
}

// IAccessible functions not supported.

STDMETHODIMP AXPlatformNodeWin::get_accSelection(VARIANT* selected) {
  COM_OBJECT_VALIDATE_1_ARG(selected);
  if (selected)
    selected->vt = VT_EMPTY;
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeWin::accSelect(
    LONG flagsSelect, VARIANT var_id) {
  COM_OBJECT_VALIDATE_VAR_ID(var_id);
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeWin::get_accHelpTopic(
    BSTR* help_file, VARIANT var_id, LONG* topic_id) {
  COM_OBJECT_VALIDATE_VAR_ID_2_ARGS(var_id, help_file, topic_id);
  if (help_file) {
    *help_file = nullptr;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeWin::put_accName(
    VARIANT var_id, BSTR put_name) {
  COM_OBJECT_VALIDATE_VAR_ID(var_id);
  // Deprecated.
  return E_NOTIMPL;
}

//
// IAccessible2 implementation.
//

STDMETHODIMP AXPlatformNodeWin::role(LONG* role) {
  COM_OBJECT_VALIDATE_1_ARG(role);
  *role = MSAARole();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_states(AccessibleStates* states) {
  COM_OBJECT_VALIDATE_1_ARG(states);
  // There are only a couple of states we need to support
  // in IAccessible2. If any more are added, we may want to
  // add a helper function like MSAAState.
  *states = IA2_STATE_OPAQUE;
  if (GetData().state & (1 << ui::AX_STATE_EDITABLE))
    *states |= IA2_STATE_EDITABLE;

  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_uniqueID(LONG* unique_id) {
  COM_OBJECT_VALIDATE_1_ARG(unique_id);
  *unique_id = -unique_id_;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_windowHandle(HWND* window_handle) {
  COM_OBJECT_VALIDATE_1_ARG(window_handle);
  *window_handle = delegate_->GetTargetForNativeAccessibilityEvent();
  return *window_handle ? S_OK : S_FALSE;
}

STDMETHODIMP AXPlatformNodeWin::get_relationTargetsOfType(
    BSTR type_bstr,
    long max_targets,
    IUnknown ***targets,
    long *n_targets) {
  COM_OBJECT_VALIDATE_2_ARGS(targets, n_targets);

  *n_targets = 0;
  *targets = nullptr;

  // Only respond to requests for relations of type "alerts".
  base::string16 type(type_bstr);
  if (type != L"alerts")
    return S_FALSE;

  // Collect all of the objects that have had an alert fired on them that
  // are a descendant of this object.
  std::vector<AXPlatformNodeWin*> alert_targets;
  for (auto iter = g_alert_targets.Get().begin();
       iter != g_alert_targets.Get().end();
       ++iter) {
    AXPlatformNodeWin* target = *iter;
    if (IsDescendant(target))
      alert_targets.push_back(target);
  }

  long count = static_cast<long>(alert_targets.size());
  if (count == 0)
    return S_FALSE;

  // Don't return more targets than max_targets - but note that the caller
  // is allowed to specify max_targets=0 to mean no limit.
  if (max_targets > 0 && count > max_targets)
    count = max_targets;

  // Return the number of targets.
  *n_targets = count;

  // Allocate COM memory for the result array and populate it.
  *targets = static_cast<IUnknown**>(
      CoTaskMemAlloc(count * sizeof(IUnknown*)));
  for (long i = 0; i < count; ++i) {
    (*targets)[i] = static_cast<IAccessible*>(alert_targets[i]);
    (*targets)[i]->AddRef();
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_attributes(BSTR* attributes) {
  COM_OBJECT_VALIDATE_1_ARG(attributes);
  base::string16 attributes_str;

  // Text fields need to report the attribute "text-model:a1" to instruct
  // screen readers to use IAccessible2 APIs to handle text editing in this
  // object (as opposed to treating it like a native Windows text box).
  // The text-model:a1 attribute is documented here:
  // http://www.linuxfoundation.org/collaborate/workgroups/accessibility/ia2/ia2_implementation_guide
  if (GetData().role == ui::AX_ROLE_TEXT_FIELD) {
    attributes_str = L"text-model:a1;";
  }

  *attributes = SysAllocString(attributes_str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_indexInParent(LONG* index_in_parent) {
  COM_OBJECT_VALIDATE_1_ARG(index_in_parent);
  *index_in_parent = GetIndexInParent();
  if (*index_in_parent < 0)
    return E_FAIL;

  return S_OK;
}

//
// IAccessible2 methods not implemented.
//

STDMETHODIMP AXPlatformNodeWin::get_attribute(BSTR name, VARIANT* attribute) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_extendedRole(BSTR* extended_role) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_nRelations(LONG* n_relations) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_relation(LONG relation_index,
                                             IAccessibleRelation** relation) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_relations(LONG max_relations,
                                              IAccessibleRelation** relations,
                                              LONG* n_relations) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::scrollTo(enum IA2ScrollType scroll_type) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::scrollToPoint(
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_groupPosition(LONG* group_level,
                                                  LONG* similar_items_in_group,
                                                  LONG* position_in_group) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_localizedExtendedRole(
    BSTR* localized_extended_role) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_nExtendedStates(LONG* n_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_extendedStates(LONG max_extended_states,
                                                   BSTR** extended_states,
                                                   LONG* n_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_localizedExtendedStates(
    LONG max_localized_extended_states,
    BSTR** localized_extended_states,
    LONG* n_localized_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_locale(IA2Locale* locale) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_accessibleWithCaret(IUnknown** accessible,
                                                        long* caret_offset) {
  return E_NOTIMPL;
}

//
// IAccessibleText
//

STDMETHODIMP AXPlatformNodeWin::get_nCharacters(LONG* n_characters) {
  COM_OBJECT_VALIDATE_1_ARG(n_characters);
  base::string16 text = TextForIAccessibleText();
  *n_characters = static_cast<LONG>(text.size());

  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_caretOffset(LONG* offset) {
  COM_OBJECT_VALIDATE_1_ARG(offset);
  *offset = static_cast<LONG>(GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END));
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_nSelections(LONG* n_selections) {
  COM_OBJECT_VALIDATE_1_ARG(n_selections);
  int sel_start = GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START);
  int sel_end = GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END);
  if (sel_start != sel_end)
    *n_selections = 1;
  else
    *n_selections = 0;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_selection(LONG selection_index,
                                              LONG* start_offset,
                                              LONG* end_offset) {
  COM_OBJECT_VALIDATE_2_ARGS(start_offset, end_offset);
  if (selection_index != 0)
    return E_INVALIDARG;

  *start_offset = static_cast<LONG>(
      GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START));
  *end_offset = static_cast<LONG>(
      GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END));
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_text(LONG start_offset,
                                         LONG end_offset,
                                         BSTR* text) {
  COM_OBJECT_VALIDATE_1_ARG(text);
  int sel_end = GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START);
  base::string16 text_str = TextForIAccessibleText();
  LONG len = static_cast<LONG>(text_str.size());

  if (start_offset == IA2_TEXT_OFFSET_LENGTH) {
    start_offset = len;
  } else if (start_offset == IA2_TEXT_OFFSET_CARET) {
    start_offset = static_cast<LONG>(sel_end);
  }
  if (end_offset == IA2_TEXT_OFFSET_LENGTH) {
    end_offset = static_cast<LONG>(text_str.size());
  } else if (end_offset == IA2_TEXT_OFFSET_CARET) {
    end_offset = static_cast<LONG>(sel_end);
  }

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset) {
    LONG tmp = start_offset;
    start_offset = end_offset;
    end_offset = tmp;
  }

  // The spec does not allow the start or end offsets to be out or range;
  // we must return an error if so.
  if (start_offset < 0)
    return E_INVALIDARG;
  if (end_offset > len)
    return E_INVALIDARG;

  base::string16 substr =
      text_str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  COM_OBJECT_VALIDATE_3_ARGS(start_offset, end_offset, text);
  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screen reader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP AXPlatformNodeWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP AXPlatformNodeWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP AXPlatformNodeWin::get_offsetAtPoint(
    LONG x, LONG y, enum IA2CoordinateType coord_type, LONG* offset) {
  COM_OBJECT_VALIDATE_1_ARG(offset);
  // We don't support this method, but we have to return something
  // rather than E_NOTIMPL or screen readers will complain.
  *offset = 0;
  return S_OK;
}

//
// IAccessibleText methods not implemented.
//

STDMETHODIMP AXPlatformNodeWin::get_newText(IA2TextSegment* new_text) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_oldText(IA2TextSegment* old_text) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::addSelection(LONG start_offset,
                                             LONG end_offset) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_attributes(LONG offset,
                                               LONG* start_offset,
                                               LONG* end_offset,
                                               BSTR* text_attributes) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::get_characterExtents(
    LONG offset,
    enum IA2CoordinateType coord_type,
    LONG* x,
    LONG* y,
    LONG* width,
    LONG* height) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::removeSelection(LONG selection_index) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::setCaretOffset(LONG offset) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::setSelection(LONG selection_index,
                                             LONG start_offset,
                                             LONG end_offset) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    enum IA2ScrollType scroll_type) {
  return E_NOTIMPL;
}
STDMETHODIMP AXPlatformNodeWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  return E_NOTIMPL;
}

//
// IServiceProvider implementation.
//

STDMETHODIMP AXPlatformNodeWin::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  COM_OBJECT_VALIDATE_1_ARG(object);

  if (riid == IID_IAccessible2) {
    FOR_EACH_OBSERVER(IAccessible2UsageObserver,
                      GetIAccessible2UsageObserverList(),
                      OnIAccessible2Used());
  }

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessible2_2 ||
      guidService == IID_IAccessibleText) {
    return QueryInterface(riid, object);
  }

  *object = nullptr;
  return E_FAIL;
}

//
// Private member functions.
//

bool AXPlatformNodeWin::IsValidId(const VARIANT& child) const {
  // Since we have a separate IAccessible COM object for each node, we only
  // support the CHILDID_SELF id.
  return (VT_I4 == child.vt) && (CHILDID_SELF == child.lVal);
}

int AXPlatformNodeWin::MSAARole() {
  switch (GetData().role) {
    case ui::AX_ROLE_ALERT:
      return ROLE_SYSTEM_ALERT;
    case ui::AX_ROLE_APPLICATION:
      return ROLE_SYSTEM_APPLICATION;
    case ui::AX_ROLE_BUTTON_DROP_DOWN:
      return ROLE_SYSTEM_BUTTONDROPDOWN;
    case ui::AX_ROLE_POP_UP_BUTTON:
      return ROLE_SYSTEM_BUTTONMENU;
    case ui::AX_ROLE_CHECK_BOX:
      return ROLE_SYSTEM_CHECKBUTTON;
    case ui::AX_ROLE_COMBO_BOX:
      return ROLE_SYSTEM_COMBOBOX;
    case ui::AX_ROLE_DIALOG:
      return ROLE_SYSTEM_DIALOG;
    case ui::AX_ROLE_GROUP:
      return ROLE_SYSTEM_GROUPING;
    case ui::AX_ROLE_IMAGE:
      return ROLE_SYSTEM_GRAPHIC;
    case ui::AX_ROLE_LINK:
      return ROLE_SYSTEM_LINK;
    case ui::AX_ROLE_LOCATION_BAR:
      return ROLE_SYSTEM_GROUPING;
    case ui::AX_ROLE_MENU_BAR:
      return ROLE_SYSTEM_MENUBAR;
    case ui::AX_ROLE_MENU_ITEM:
      return ROLE_SYSTEM_MENUITEM;
    case ui::AX_ROLE_MENU_LIST_POPUP:
      return ROLE_SYSTEM_MENUPOPUP;
    case ui::AX_ROLE_TREE:
      return ROLE_SYSTEM_OUTLINE;
    case ui::AX_ROLE_TREE_ITEM:
      return ROLE_SYSTEM_OUTLINEITEM;
    case ui::AX_ROLE_TAB:
      return ROLE_SYSTEM_PAGETAB;
    case ui::AX_ROLE_TAB_LIST:
      return ROLE_SYSTEM_PAGETABLIST;
    case ui::AX_ROLE_PANE:
      return ROLE_SYSTEM_PANE;
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      return ROLE_SYSTEM_PROGRESSBAR;
    case ui::AX_ROLE_BUTTON:
      return ROLE_SYSTEM_PUSHBUTTON;
    case ui::AX_ROLE_RADIO_BUTTON:
      return ROLE_SYSTEM_RADIOBUTTON;
    case ui::AX_ROLE_SCROLL_BAR:
      return ROLE_SYSTEM_SCROLLBAR;
    case ui::AX_ROLE_SPLITTER:
      return ROLE_SYSTEM_SEPARATOR;
    case ui::AX_ROLE_SLIDER:
      return ROLE_SYSTEM_SLIDER;
    case ui::AX_ROLE_STATIC_TEXT:
      return ROLE_SYSTEM_STATICTEXT;
    case ui::AX_ROLE_TEXT_FIELD:
      return ROLE_SYSTEM_TEXT;
    case ui::AX_ROLE_TITLE_BAR:
      return ROLE_SYSTEM_TITLEBAR;
    case ui::AX_ROLE_TOOLBAR:
      return ROLE_SYSTEM_TOOLBAR;
    case ui::AX_ROLE_WEB_VIEW:
      return ROLE_SYSTEM_GROUPING;
    case ui::AX_ROLE_WINDOW:
      return ROLE_SYSTEM_WINDOW;
    case ui::AX_ROLE_CLIENT:
    default:
      // This is the default role for MSAA.
      return ROLE_SYSTEM_CLIENT;
  }
}

int AXPlatformNodeWin::MSAAState() {
  uint32_t state = GetData().state;

  int msaa_state = 0;
  if (state & (1 << ui::AX_STATE_CHECKED))
    msaa_state |= STATE_SYSTEM_CHECKED;
  if (state & (1 << ui::AX_STATE_COLLAPSED))
    msaa_state |= STATE_SYSTEM_COLLAPSED;
  if (state & (1 << ui::AX_STATE_DEFAULT))
    msaa_state |= STATE_SYSTEM_DEFAULT;
  if (state & (1 << ui::AX_STATE_EXPANDED))
    msaa_state |= STATE_SYSTEM_EXPANDED;
  if (state & (1 << ui::AX_STATE_FOCUSABLE))
    msaa_state |= STATE_SYSTEM_FOCUSABLE;
  if (state & (1 << ui::AX_STATE_HASPOPUP))
    msaa_state |= STATE_SYSTEM_HASPOPUP;
  if (state & (1 << ui::AX_STATE_HOVERED))
    msaa_state |= STATE_SYSTEM_HOTTRACKED;
  if (state & (1 << ui::AX_STATE_INVISIBLE))
    msaa_state |= STATE_SYSTEM_INVISIBLE;
  if (state & (1 << ui::AX_STATE_LINKED))
    msaa_state |= STATE_SYSTEM_LINKED;
  if (state & (1 << ui::AX_STATE_OFFSCREEN))
    msaa_state |= STATE_SYSTEM_OFFSCREEN;
  if (state & (1 << ui::AX_STATE_PRESSED))
    msaa_state |= STATE_SYSTEM_PRESSED;
  if (state & (1 << ui::AX_STATE_PROTECTED))
    msaa_state |= STATE_SYSTEM_PROTECTED;
  if (state & (1 << ui::AX_STATE_READ_ONLY))
    msaa_state |= STATE_SYSTEM_READONLY;
  if (state & (1 << ui::AX_STATE_SELECTABLE))
    msaa_state |= STATE_SYSTEM_SELECTABLE;
  if (state & (1 << ui::AX_STATE_SELECTED))
    msaa_state |= STATE_SYSTEM_SELECTED;
  if (state & (1 << ui::AX_STATE_DISABLED))
    msaa_state |= STATE_SYSTEM_UNAVAILABLE;

  gfx::NativeViewAccessible focus = delegate_->GetFocus();
  if (focus == GetNativeViewAccessible())
    msaa_state |= STATE_SYSTEM_FOCUSED;

  // On Windows, the "focus" bit should be set on certain containers, like
  // menu bars, when visible.
  //
  // TODO(dmazzoni): this should probably check if focus is actually inside
  // the menu bar, but we don't currently track focus inside menu pop-ups,
  // and Chrome only has one menu visible at a time so this works for now.
  if (GetData().role == ui::AX_ROLE_MENU_BAR &&
      !(state & (1 << ui::AX_STATE_INVISIBLE))) {
    msaa_state |= STATE_SYSTEM_FOCUSED;
  }

  return msaa_state;
}

int AXPlatformNodeWin::MSAAEvent(ui::AXEvent event) {
  switch (event) {
    case ui::AX_EVENT_ALERT:
      return EVENT_SYSTEM_ALERT;
    case ui::AX_EVENT_FOCUS:
      return EVENT_OBJECT_FOCUS;
    case ui::AX_EVENT_MENU_START:
      return EVENT_SYSTEM_MENUSTART;
    case ui::AX_EVENT_MENU_END:
      return EVENT_SYSTEM_MENUEND;
    case ui::AX_EVENT_MENU_POPUP_START:
      return EVENT_SYSTEM_MENUPOPUPSTART;
    case ui::AX_EVENT_MENU_POPUP_END:
      return EVENT_SYSTEM_MENUPOPUPEND;
    case ui::AX_EVENT_SELECTION:
      return EVENT_OBJECT_SELECTION;
    case ui::AX_EVENT_SELECTION_ADD:
      return EVENT_OBJECT_SELECTIONADD;
    case ui::AX_EVENT_SELECTION_REMOVE:
      return EVENT_OBJECT_SELECTIONREMOVE;
    case ui::AX_EVENT_TEXT_CHANGED:
      return EVENT_OBJECT_NAMECHANGE;
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      return IA2_EVENT_TEXT_CARET_MOVED;
    case ui::AX_EVENT_VALUE_CHANGED:
      return EVENT_OBJECT_VALUECHANGE;
    default:
      return -1;
  }
}

HRESULT AXPlatformNodeWin::GetStringAttributeAsBstr(
    ui::AXStringAttribute attribute,
    BSTR* value_bstr) const {
  base::string16 str;

  if (!GetString16Attribute(attribute, &str))
    return S_FALSE;

  if (str.empty())
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

void AXPlatformNodeWin::AddAlertTarget() {
  g_alert_targets.Get().insert(this);
}

void AXPlatformNodeWin::RemoveAlertTarget() {
  if (g_alert_targets.Get().find(this) != g_alert_targets.Get().end())
    g_alert_targets.Get().erase(this);
}

base::string16 AXPlatformNodeWin::TextForIAccessibleText() {
  if (GetData().role == ui::AX_ROLE_TEXT_FIELD)
    return GetString16Attribute(ui::AX_ATTR_VALUE);
  else
    return GetString16Attribute(ui::AX_ATTR_NAME);
}

void AXPlatformNodeWin::HandleSpecialTextOffset(
    const base::string16& text, LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH) {
    *offset = static_cast<LONG>(text.size());
  } else if (*offset == IA2_TEXT_OFFSET_CARET) {
    get_caretOffset(offset);
  }
}

ui::TextBoundaryType AXPlatformNodeWin::IA2TextBoundaryToTextBoundary(
    IA2TextBoundaryType ia2_boundary) {
  switch(ia2_boundary) {
    case IA2_TEXT_BOUNDARY_CHAR: return ui::CHAR_BOUNDARY;
    case IA2_TEXT_BOUNDARY_WORD: return ui::WORD_BOUNDARY;
    case IA2_TEXT_BOUNDARY_LINE: return ui::LINE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_SENTENCE: return ui::SENTENCE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_PARAGRAPH: return ui::PARAGRAPH_BOUNDARY;
    case IA2_TEXT_BOUNDARY_ALL: return ui::ALL_BOUNDARY;
    default:
      NOTREACHED();
      return ui::CHAR_BOUNDARY;
  }
}

LONG AXPlatformNodeWin::FindBoundary(
    const base::string16& text,
    IA2TextBoundaryType ia2_boundary,
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  HandleSpecialTextOffset(text, &start_offset);
  ui::TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
  std::vector<int32_t> line_breaks;
  return static_cast<LONG>(ui::FindAccessibleTextBoundary(
      text, line_breaks, boundary, start_offset, direction));
}

}  // namespace ui
