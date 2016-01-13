// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility_win.h"

#include <oleacc.h>
#include <UIAutomationClient.h>

#include <set>
#include <vector>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/win/accessibility_ids_win.h"
#include "ui/base/win/accessibility_misc_utils.h"
#include "ui/base/win/atl_module.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace views {
namespace {

class AccessibleWebViewRegistry {
 public:
  static AccessibleWebViewRegistry* GetInstance();

  void RegisterWebView(View* web_view);

  void UnregisterWebView(View* web_view);

  // Given the view that received the request for the accessible
  // id in |top_view|, and the child id requested, return the native
  // accessible object with that child id from one of the WebViews in
  // |top_view|'s view hierarchy, if any.
  IAccessible* GetAccessibleFromWebView(View* top_view, long child_id);

  // The system uses IAccessible APIs for many purposes, but only
  // assistive technology like screen readers uses IAccessible2.
  // Call this method to note that the IAccessible2 interface was queried and
  // that WebViews should be proactively notified that this interface will be
  // used. If this is enabled for the first time, this will explicitly call
  // QueryService with an argument of IAccessible2 on all WebViews, otherwise
  // it will just do it from now on.
  void EnableIAccessible2Support();

 private:
  friend struct DefaultSingletonTraits<AccessibleWebViewRegistry>;
  AccessibleWebViewRegistry();
  ~AccessibleWebViewRegistry() {}

  IAccessible* AccessibleObjectFromChildId(View* web_view, long child_id);

  void QueryIAccessible2Interface(View* web_view);

  // Set of all web views. We check whether each one is contained in a
  // top view dynamically rather than keeping track of a map.
  std::set<View*> web_views_;

  // The most recent top view used in a call to GetAccessibleFromWebView.
  View* last_top_view_;

  // The most recent web view where an accessible object was found,
  // corresponding to |last_top_view_|.
  View* last_web_view_;

  // If IAccessible2 support is enabled, we query the IAccessible2 interface
  // of WebViews proactively when they're registered, so that they are
  // aware that they need to support this interface.
  bool iaccessible2_support_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleWebViewRegistry);
};

AccessibleWebViewRegistry::AccessibleWebViewRegistry()
    : last_top_view_(NULL),
      last_web_view_(NULL),
      iaccessible2_support_enabled_(false) {
}

AccessibleWebViewRegistry* AccessibleWebViewRegistry::GetInstance() {
  return Singleton<AccessibleWebViewRegistry>::get();
}

void AccessibleWebViewRegistry::RegisterWebView(View* web_view) {
  DCHECK(web_views_.find(web_view) == web_views_.end());
  web_views_.insert(web_view);

  if (iaccessible2_support_enabled_)
    QueryIAccessible2Interface(web_view);
}

void AccessibleWebViewRegistry::UnregisterWebView(View* web_view) {
  DCHECK(web_views_.find(web_view) != web_views_.end());
  web_views_.erase(web_view);
  if (last_web_view_ == web_view) {
    last_top_view_ = NULL;
    last_web_view_ = NULL;
  }
}

IAccessible* AccessibleWebViewRegistry::GetAccessibleFromWebView(
    View* top_view, long child_id) {
  // This function gets called frequently, so try to avoid searching all
  // of the web views if the notification is on the same web view that
  // sent the last one.
  if (last_top_view_ == top_view) {
    IAccessible* accessible =
        AccessibleObjectFromChildId(last_web_view_, child_id);
    if (accessible)
      return accessible;
  }

  // Search all web views. For each one, first ensure it's a descendant
  // of this view where the event was posted - and if so, see if it owns
  // an accessible object with that child id. If so, save the view to speed
  // up the next notification.
  for (std::set<View*>::iterator iter = web_views_.begin();
       iter != web_views_.end(); ++iter) {
    View* web_view = *iter;
    if (top_view == web_view || !top_view->Contains(web_view))
      continue;
    IAccessible* accessible = AccessibleObjectFromChildId(web_view, child_id);
    if (accessible) {
      last_top_view_ = top_view;
      last_web_view_ = web_view;
      return accessible;
    }
  }

  return NULL;
}

void AccessibleWebViewRegistry::EnableIAccessible2Support() {
  if (iaccessible2_support_enabled_)
    return;
  iaccessible2_support_enabled_ = true;
  for (std::set<View*>::iterator iter = web_views_.begin();
       iter != web_views_.end(); ++iter) {
    QueryIAccessible2Interface(*iter);
  }
}

IAccessible* AccessibleWebViewRegistry::AccessibleObjectFromChildId(
    View* web_view,
    long child_id) {
  IAccessible* web_view_accessible = web_view->GetNativeViewAccessible();
  if (web_view_accessible == NULL)
    return NULL;

  VARIANT var_child;
  var_child.vt = VT_I4;
  var_child.lVal = child_id;
  IAccessible* result = NULL;
  if (S_OK == web_view_accessible->get_accChild(
      var_child, reinterpret_cast<IDispatch**>(&result))) {
    return result;
  }

  return NULL;
}

void AccessibleWebViewRegistry::QueryIAccessible2Interface(View* web_view) {
  IAccessible* web_view_accessible = web_view->GetNativeViewAccessible();
  if (!web_view_accessible)
    return;

  base::win::ScopedComPtr<IServiceProvider> service_provider;
  if (S_OK != web_view_accessible->QueryInterface(service_provider.Receive()))
    return;
  base::win::ScopedComPtr<IAccessible2> iaccessible2;
  service_provider->QueryService(
      IID_IAccessible, IID_IAccessible2,
      reinterpret_cast<void**>(iaccessible2.Receive()));
}

}  // anonymous namespace

// static
long NativeViewAccessibilityWin::next_unique_id_ = 1;
int NativeViewAccessibilityWin::view_storage_ids_[kMaxViewStorageIds] = {0};
int NativeViewAccessibilityWin::next_view_storage_id_index_ = 0;
std::vector<int> NativeViewAccessibilityWin::alert_target_view_storage_ids_;

// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  // Make sure ATL is initialized in this module.
  ui::win::CreateATLModuleIfNeeded();

  CComObject<NativeViewAccessibilityWin>* instance = NULL;
  HRESULT hr = CComObject<NativeViewAccessibilityWin>::CreateInstance(
      &instance);
  DCHECK(SUCCEEDED(hr));
  instance->set_view(view);
  instance->AddRef();
  return instance;
}

NativeViewAccessibilityWin::NativeViewAccessibilityWin()
    : view_(NULL),
      unique_id_(next_unique_id_++) {
}

NativeViewAccessibilityWin::~NativeViewAccessibilityWin() {
  RemoveAlertTarget();
}

void NativeViewAccessibilityWin::NotifyAccessibilityEvent(
    ui::AXEvent event_type) {
  if (!view_)
    return;

  ViewStorage* view_storage = ViewStorage::GetInstance();
  HWND hwnd = HWNDForView(view_);
  int view_storage_id = view_storage_ids_[next_view_storage_id_index_];
  if (view_storage_id == 0) {
    view_storage_id = view_storage->CreateStorageID();
    view_storage_ids_[next_view_storage_id_index_] = view_storage_id;
  } else {
    view_storage->RemoveView(view_storage_id);
  }
  view_storage->StoreView(view_storage_id, view_);

  // Positive child ids are used for enumerating direct children,
  // negative child ids can be used as unique ids to refer to a specific
  // descendants.  Make index into view_storage_ids_ into a negative child id.
  int child_id =
      base::win::kFirstViewsAccessibilityId - next_view_storage_id_index_;
  ::NotifyWinEvent(MSAAEvent(event_type), hwnd, OBJID_CLIENT, child_id);
  next_view_storage_id_index_ =
      (next_view_storage_id_index_ + 1) % kMaxViewStorageIds;

  // Keep track of views that are a target of an alert event.
  if (event_type == ui::AX_EVENT_ALERT)
    AddAlertTarget();
}

gfx::NativeViewAccessible NativeViewAccessibilityWin::GetNativeObject() {
  return this;
}

void NativeViewAccessibilityWin::Destroy() {
  view_ = NULL;
  Release();
}

STDMETHODIMP NativeViewAccessibilityWin::accHitTest(
    LONG x_left, LONG y_top, VARIANT* child) {
  if (!child)
    return E_INVALIDARG;

  if (!view_ || !view_->GetWidget())
    return E_FAIL;

  // If this is a root view, our widget might have child widgets.
  // Search child widgets first, since they're on top in the z-order.
  if (view_->GetWidget()->GetRootView() == view_) {
    std::vector<Widget*> child_widgets;
    PopulateChildWidgetVector(&child_widgets);
    for (size_t i = 0; i < child_widgets.size(); ++i) {
      Widget* child_widget = child_widgets[i];
      IAccessible* child_accessible =
          child_widget->GetRootView()->GetNativeViewAccessible();
      HRESULT result = child_accessible->accHitTest(x_left, y_top, child);
      if (result == S_OK)
        return result;
    }
  }

  gfx::Point point(x_left, y_top);
  View::ConvertPointFromScreen(view_, &point);

  // If the point is not inside this view, return false.
  if (!view_->HitTestPoint(point)) {
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  // Check if the point is within any of the immediate children of this
  // view.
  View* hit_child_view = NULL;
  for (int i = view_->child_count() - 1; i >= 0; --i) {
    View* child_view = view_->child_at(i);
    if (!child_view->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    view_->ConvertPointToTarget(view_, child_view, &point_in_child_coords);
    if (child_view->HitTestPoint(point_in_child_coords)) {
      hit_child_view = child_view;
      break;
    }
  }

  // If the point was within one of this view's immediate children,
  // call accHitTest recursively on that child's native view accessible -
  // which may be a recursive call to this function or it may be overridden,
  // for example in the case of a WebView.
  if (hit_child_view) {
    HRESULT result = hit_child_view->GetNativeViewAccessible()->accHitTest(
        x_left, y_top, child);

    // If the recursive call returned CHILDID_SELF, we have to convert that
    // into a VT_DISPATCH for the return value to this call.
    if (S_OK == result && child->vt == VT_I4 && child->lVal == CHILDID_SELF) {
      child->vt = VT_DISPATCH;
      child->pdispVal = hit_child_view->GetNativeViewAccessible();
      // Always increment ref when returning a reference to a COM object.
      child->pdispVal->AddRef();
    }
    return result;
  }

  // This object is the best match, so return CHILDID_SELF. It's tempting to
  // simplify the logic and use VT_DISPATCH everywhere, but the Windows
  // call AccessibleObjectFromPoint will keep calling accHitTest until some
  // object returns CHILDID_SELF.
  child->vt = VT_I4;
  child->lVal = CHILDID_SELF;
  return S_OK;
}

HRESULT NativeViewAccessibilityWin::accDoDefaultAction(VARIANT var_id) {
  if (!IsValidId(var_id))
    return E_INVALIDARG;

  // The object does not support the method. This value is returned for
  // controls that do not perform actions, such as edit fields.
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP NativeViewAccessibilityWin::accLocation(
    LONG* x_left, LONG* y_top, LONG* width, LONG* height, VARIANT var_id) {
  if (!IsValidId(var_id) || !x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  if (!view_->bounds().IsEmpty()) {
    *width  = view_->width();
    *height = view_->height();
    gfx::Point topleft(view_->bounds().origin());
    View::ConvertPointToScreen(
        view_->parent() ? view_->parent() : view_, &topleft);
    *x_left = topleft.x();
    *y_top  = topleft.y();
  } else {
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::accNavigate(
    LONG nav_dir, VARIANT start, VARIANT* end) {
  if (start.vt != VT_I4 || !end)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  switch (nav_dir) {
    case NAVDIR_FIRSTCHILD:
    case NAVDIR_LASTCHILD: {
      if (start.lVal != CHILDID_SELF) {
        // Start of navigation must be on the View itself.
        return E_INVALIDARG;
      } else if (!view_->has_children()) {
        // No children found.
        return S_FALSE;
      }

      // Set child_id based on first or last child.
      int child_id = 0;
      if (nav_dir == NAVDIR_LASTCHILD)
        child_id = view_->child_count() - 1;

      View* child = view_->child_at(child_id);
      end->vt = VT_DISPATCH;
      end->pdispVal = child->GetNativeViewAccessible();
      end->pdispVal->AddRef();
      return S_OK;
    }
    case NAVDIR_LEFT:
    case NAVDIR_UP:
    case NAVDIR_PREVIOUS:
    case NAVDIR_RIGHT:
    case NAVDIR_DOWN:
    case NAVDIR_NEXT: {
      // Retrieve parent to access view index and perform bounds checking.
      View* parent = view_->parent();
      if (!parent) {
        return E_FAIL;
      }

      if (start.lVal == CHILDID_SELF) {
        int view_index = parent->GetIndexOf(view_);
        // Check navigation bounds, adjusting for View child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, view_index, -1,
                        parent->child_count() - 1)) {
          // Navigation attempted to go out-of-bounds.
          end->vt = VT_EMPTY;
          return S_FALSE;
        } else {
          if (IsNavDirNext(nav_dir)) {
            view_index += 1;
          } else {
            view_index -=1;
          }
        }

        View* child = parent->child_at(view_index);
        end->pdispVal = child->GetNativeViewAccessible();
        end->vt = VT_DISPATCH;
        end->pdispVal->AddRef();
        return S_OK;
      } else {
        // Check navigation bounds, adjusting for MSAA child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, start.lVal, 0, parent->child_count() + 1)) {
            // Navigation attempted to go out-of-bounds.
            end->vt = VT_EMPTY;
            return S_FALSE;
          } else {
            if (IsNavDirNext(nav_dir)) {
              start.lVal += 1;
            } else {
              start.lVal -= 1;
            }
        }

        HRESULT result = this->get_accChild(start, &end->pdispVal);
        if (result == S_FALSE) {
          // Child is a leaf.
          end->vt = VT_I4;
          end->lVal = start.lVal;
        } else if (result == E_INVALIDARG) {
          return E_INVALIDARG;
        } else {
          // Child is not a leaf.
          end->vt = VT_DISPATCH;
        }
      }
      break;
    }
    default:
      return E_INVALIDARG;
  }
  // Navigation performed correctly. Global return for this function, if no
  // error triggered an escape earlier.
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accChild(VARIANT var_child,
                                                      IDispatch** disp_child) {
  if (var_child.vt != VT_I4 || !disp_child)
    return E_INVALIDARG;

  if (!view_ || !view_->GetWidget())
    return E_FAIL;

  LONG child_id = V_I4(&var_child);

  if (child_id == CHILDID_SELF) {
    // Remain with the same dispatch.
    return S_OK;
  }

  // If this is a root view, our widget might have child widgets. Include
  std::vector<Widget*> child_widgets;
  if (view_->GetWidget()->GetRootView() == view_)
    PopulateChildWidgetVector(&child_widgets);
  int child_widget_count = static_cast<int>(child_widgets.size());

  View* child_view = NULL;
  if (child_id > 0) {
    // Positive child ids are a 1-based child index, used by clients
    // that want to enumerate all immediate children.
    int child_id_as_index = child_id - 1;
    if (child_id_as_index < view_->child_count()) {
      child_view = view_->child_at(child_id_as_index);
    } else if (child_id_as_index < view_->child_count() + child_widget_count) {
      Widget* child_widget =
          child_widgets[child_id_as_index - view_->child_count()];
      child_view = child_widget->GetRootView();
    }
  } else {
    // Negative child ids can be used to map to any descendant.
    // Check child widget first.
    for (int i = 0; i < child_widget_count; i++) {
      Widget* child_widget = child_widgets[i];
      IAccessible* child_accessible =
          child_widget->GetRootView()->GetNativeViewAccessible();
      HRESULT result = child_accessible->get_accChild(var_child, disp_child);
      if (result == S_OK)
        return result;
    }

    // We map child ids to a view storage id that can refer to a
    // specific view (if that view still exists).
    int view_storage_id_index =
        base::win::kFirstViewsAccessibilityId - child_id;
    if (view_storage_id_index >= 0 &&
        view_storage_id_index < kMaxViewStorageIds) {
      int view_storage_id = view_storage_ids_[view_storage_id_index];
      ViewStorage* view_storage = ViewStorage::GetInstance();
      child_view = view_storage->RetrieveView(view_storage_id);
    } else {
      *disp_child = AccessibleWebViewRegistry::GetInstance()->
          GetAccessibleFromWebView(view_, child_id);
      if (*disp_child)
        return S_OK;
    }
  }

  if (!child_view) {
    // No child found.
    *disp_child = NULL;
    return E_FAIL;
  }

  *disp_child = child_view->GetNativeViewAccessible();
  (*disp_child)->AddRef();
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accChildCount(LONG* child_count) {
  if (!child_count)
    return E_INVALIDARG;

  if (!view_ || !view_->GetWidget())
    return E_FAIL;

  *child_count = view_->child_count();

  // If this is a root view, our widget might have child widgets. Include
  // them, too.
  if (view_->GetWidget()->GetRootView() == view_) {
    std::vector<Widget*> child_widgets;
    PopulateChildWidgetVector(&child_widgets);
    *child_count += child_widgets.size();
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accDefaultAction(
    VARIANT var_id, BSTR* def_action) {
  if (!IsValidId(var_id) || !def_action)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  base::string16 temp_action = state.default_action;

  if (!temp_action.empty()) {
    *def_action = SysAllocString(temp_action.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accDescription(
    VARIANT var_id, BSTR* desc) {
  if (!IsValidId(var_id) || !desc)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  base::string16 temp_desc;

  view_->GetTooltipText(gfx::Point(), &temp_desc);
  if (!temp_desc.empty()) {
    *desc = SysAllocString(temp_desc.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accFocus(VARIANT* focus_child) {
  if (!focus_child)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  FocusManager* focus_manager = view_->GetFocusManager();
  View* focus = focus_manager ? focus_manager->GetFocusedView() : NULL;
  if (focus == view_) {
    // This view has focus.
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus && view_->Contains(focus)) {
    // Return the child object that has the keyboard focus.
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus->GetNativeViewAccessible();
    focus_child->pdispVal->AddRef();
    return S_OK;
  } else {
    // Neither this object nor any of its children has the keyboard focus.
    focus_child->vt = VT_EMPTY;
  }
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accKeyboardShortcut(
    VARIANT var_id, BSTR* acc_key) {
  if (!IsValidId(var_id) || !acc_key)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  base::string16 temp_key = state.keyboard_shortcut;

  if (!temp_key.empty()) {
    *acc_key = SysAllocString(temp_key.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accName(
    VARIANT var_id, BSTR* name) {
  if (!IsValidId(var_id) || !name)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  // Retrieve the current view's name.
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  base::string16 temp_name = state.name;
  if (!temp_name.empty()) {
    // Return name retrieved.
    *name = SysAllocString(temp_name.c_str());
  } else {
    // If view has no name, return S_FALSE.
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accParent(
    IDispatch** disp_parent) {
  if (!disp_parent)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  *disp_parent = NULL;
  View* parent_view = view_->parent();

  if (!parent_view) {
    HWND hwnd = HWNDForView(view_);
    if (!hwnd)
      return S_FALSE;

    return ::AccessibleObjectFromWindow(
        hwnd, OBJID_WINDOW, IID_IAccessible,
        reinterpret_cast<void**>(disp_parent));
  }

  *disp_parent = parent_view->GetNativeViewAccessible();
  (*disp_parent)->AddRef();
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accRole(
    VARIANT var_id, VARIANT* role) {
  if (!IsValidId(var_id) || !role)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  role->vt = VT_I4;
  role->lVal = MSAARole(state.role);
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accState(
    VARIANT var_id, VARIANT* state) {
  // This returns MSAA states. See also the IAccessible2 interface
  // get_states().

  if (!IsValidId(var_id) || !state)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  state->vt = VT_I4;

  // Retrieve all currently applicable states of the parent.
  SetState(state, view_);

  // Make sure that state is not empty, and has the proper type.
  if (state->vt == VT_EMPTY)
    return E_FAIL;

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accValue(VARIANT var_id,
                                                      BSTR* value) {
  if (!IsValidId(var_id) || !value)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  // Retrieve the current view's value.
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  base::string16 temp_value = state.value;

  if (!temp_value.empty()) {
    // Return value retrieved.
    *value = SysAllocString(temp_value.c_str());
  } else {
    // If view has no value, fall back into the default implementation.
    *value = NULL;
    return E_NOTIMPL;
  }

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::put_accValue(VARIANT var_id,
                                                      BSTR new_value) {
  if (!IsValidId(var_id) || !new_value)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  // Return an error if the view can't set the value.
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  if (state.set_value_callback.is_null())
    return E_FAIL;

  state.set_value_callback.Run(new_value);
  return S_OK;
}

// IAccessible functions not supported.

STDMETHODIMP NativeViewAccessibilityWin::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;
  return E_NOTIMPL;
}

STDMETHODIMP NativeViewAccessibilityWin::accSelect(
    LONG flagsSelect, VARIANT var_id) {
  return E_NOTIMPL;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accHelp(
    VARIANT var_id, BSTR* help) {
  base::string16 temp = base::UTF8ToUTF16(view_->GetClassName());
  *help = SysAllocString(temp.c_str());
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_accHelpTopic(
    BSTR* help_file, VARIANT var_id, LONG* topic_id) {
  if (help_file) {
    *help_file = NULL;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return E_NOTIMPL;
}

STDMETHODIMP NativeViewAccessibilityWin::put_accName(
    VARIANT var_id, BSTR put_name) {
  // Deprecated.
  return E_NOTIMPL;
}

//
// IAccessible2
//

STDMETHODIMP NativeViewAccessibilityWin::role(LONG* role) {
  if (!view_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  *role = MSAARole(state.role);
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_states(AccessibleStates* states) {
  // This returns IAccessible2 states, which supplement MSAA states.
  // See also the MSAA interface get_accState.

  if (!view_)
    return E_FAIL;

  if (!states)
    return E_INVALIDARG;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);

  // There are only a couple of states we need to support
  // in IAccessible2. If any more are added, we may want to
  // add a helper function like MSAAState.
  *states = IA2_STATE_OPAQUE;
  if (state.HasStateFlag(ui::AX_STATE_EDITABLE))
    *states |= IA2_STATE_EDITABLE;

  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_uniqueID(LONG* unique_id) {
  if (!view_)
    return E_FAIL;

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = unique_id_;
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_windowHandle(HWND* window_handle) {
  if (!view_)
    return E_FAIL;

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle = HWNDForView(view_);
  return *window_handle ? S_OK : S_FALSE;
}

STDMETHODIMP NativeViewAccessibilityWin::get_relationTargetsOfType(
    BSTR type_bstr,
    long max_targets,
    IUnknown ***targets,
    long *n_targets) {
  if (!view_)
    return E_FAIL;

  if (!targets || !n_targets)
    return E_INVALIDARG;

  *n_targets = 0;
  *targets = NULL;

  // Only respond to requests for relations of type "alerts" on the
  // root view.
  base::string16 type(type_bstr);
  if (type != L"alerts" || view_->parent())
    return S_FALSE;

  // Collect all of the alert views that are still valid.
  std::vector<View*> alert_views;
  ViewStorage* view_storage = ViewStorage::GetInstance();
  for (size_t i = 0; i < alert_target_view_storage_ids_.size(); ++i) {
    int view_storage_id = alert_target_view_storage_ids_[i];
    View* view = view_storage->RetrieveView(view_storage_id);
    if (!view || !view_->Contains(view))
      continue;
    alert_views.push_back(view);
  }

  long count = alert_views.size();
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
    (*targets)[i] = alert_views[i]->GetNativeViewAccessible();
    (*targets)[i]->AddRef();
  }
  return S_OK;
}

//
// IAccessibleText
//

STDMETHODIMP NativeViewAccessibilityWin::get_nCharacters(LONG* n_characters) {
  if (!view_)
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  base::string16 text = TextForIAccessibleText();
  *n_characters = static_cast<LONG>(text.size());
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_caretOffset(LONG* offset) {
  if (!view_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  *offset = static_cast<LONG>(state.selection_end);
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_nSelections(LONG* n_selections) {
  if (!view_)
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  if (state.selection_start != state.selection_end)
    *n_selections = 1;
  else
    *n_selections = 0;
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_selection(LONG selection_index,
                                                      LONG* start_offset,
                                                      LONG* end_offset) {
  if (!view_)
    return E_FAIL;

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  *start_offset = static_cast<LONG>(state.selection_start);
  *end_offset = static_cast<LONG>(state.selection_end);
  return S_OK;
}

STDMETHODIMP NativeViewAccessibilityWin::get_text(LONG start_offset,
                                                  LONG end_offset,
                                                  BSTR* text) {
  if (!view_)
    return E_FAIL;

  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  base::string16 text_str = TextForIAccessibleText();
  LONG len = static_cast<LONG>(text_str.size());

  if (start_offset == IA2_TEXT_OFFSET_LENGTH) {
    start_offset = len;
  } else if (start_offset == IA2_TEXT_OFFSET_CARET) {
    start_offset = static_cast<LONG>(state.selection_end);
  }
  if (end_offset == IA2_TEXT_OFFSET_LENGTH) {
    end_offset = static_cast<LONG>(text_str.size());
  } else if (end_offset == IA2_TEXT_OFFSET_CARET) {
    end_offset = static_cast<LONG>(state.selection_end);
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

STDMETHODIMP NativeViewAccessibilityWin::get_textAtOffset(
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
    *text = NULL;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP NativeViewAccessibilityWin::get_textBeforeOffset(
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
    *text = NULL;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP NativeViewAccessibilityWin::get_textAfterOffset(
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
    *text = NULL;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP NativeViewAccessibilityWin::get_offsetAtPoint(
    LONG x, LONG y, enum IA2CoordinateType coord_type, LONG* offset) {
  if (!view_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // We don't support this method, but we have to return something
  // rather than E_NOTIMPL or screen readers will complain.
  *offset = 0;
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP NativeViewAccessibilityWin::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!view_)
    return E_FAIL;

  if (riid == IID_IAccessible2 || riid == IID_IAccessible2_2)
    AccessibleWebViewRegistry::GetInstance()->EnableIAccessible2Support();

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessible2_2 ||
      guidService == IID_IAccessibleText)  {
    return QueryInterface(riid, object);
  }

  // We only support the IAccessibleEx interface on Windows 8 and above. This
  // is needed for the On screen Keyboard to show up in metro mode, when the
  // user taps an editable region in the window.
  // All methods in the IAccessibleEx interface are unimplemented.
  if (riid == IID_IAccessibleEx &&
      base::win::GetVersion() >= base::win::VERSION_WIN8) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

STDMETHODIMP NativeViewAccessibilityWin::GetPatternProvider(
    PATTERNID id, IUnknown** provider) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for pattern id: "
           << id;
  if (id == UIA_ValuePatternId || id == UIA_TextPatternId) {
    ui::AXViewState state;
    view_->GetAccessibleState(&state);
    long role = MSAARole(state.role);

    if (role == ROLE_SYSTEM_TEXT) {
      DVLOG(1) << "Returning UIA text provider";
      base::win::UIATextProvider::CreateTextProvider(
          state.value, true, provider);
      return S_OK;
    }
  }
  return E_NOTIMPL;
}

STDMETHODIMP NativeViewAccessibilityWin::GetPropertyValue(PROPERTYID id,
                                                          VARIANT* ret) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for property id: "
           << id;
  if (id == UIA_ControlTypePropertyId) {
    ui::AXViewState state;
    view_->GetAccessibleState(&state);
    long role = MSAARole(state.role);
    if (role == ROLE_SYSTEM_TEXT) {
      V_VT(ret) = VT_I4;
      ret->lVal = UIA_EditControlTypeId;
      DVLOG(1) << "Returning Edit control type";
    } else {
      DVLOG(1) << "Returning empty control type";
      V_VT(ret) = VT_EMPTY;
    }
  } else {
    V_VT(ret) = VT_EMPTY;
  }
  return S_OK;
}

//
// Static methods.
//

void NativeViewAccessibility::RegisterWebView(View* web_view) {
  AccessibleWebViewRegistry::GetInstance()->RegisterWebView(web_view);
}

void NativeViewAccessibility::UnregisterWebView(View* web_view) {
  AccessibleWebViewRegistry::GetInstance()->UnregisterWebView(web_view);
}

int32 NativeViewAccessibilityWin::MSAAEvent(ui::AXEvent event) {
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
    case ui::AX_EVENT_TEXT_CHANGED:
      return EVENT_OBJECT_NAMECHANGE;
    case ui::AX_EVENT_SELECTION_CHANGED:
      return IA2_EVENT_TEXT_CARET_MOVED;
    case ui::AX_EVENT_VALUE_CHANGED:
      return EVENT_OBJECT_VALUECHANGE;
    default:
      // Not supported or invalid event.
      NOTREACHED();
      return -1;
  }
}

int32 NativeViewAccessibilityWin::MSAARole(ui::AXRole role) {
  switch (role) {
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
    case ui::AX_ROLE_WINDOW:
      return ROLE_SYSTEM_WINDOW;
    case ui::AX_ROLE_CLIENT:
    default:
      // This is the default role for MSAA.
      return ROLE_SYSTEM_CLIENT;
  }
}

int32 NativeViewAccessibilityWin::MSAAState(const ui::AXViewState& state) {
  // This maps MSAA states for get_accState(). See also the IAccessible2
  // interface get_states().

  int32 msaa_state = 0;
  if (state.HasStateFlag(ui::AX_STATE_CHECKED))
    msaa_state |= STATE_SYSTEM_CHECKED;
  if (state.HasStateFlag(ui::AX_STATE_COLLAPSED))
    msaa_state |= STATE_SYSTEM_COLLAPSED;
  if (state.HasStateFlag(ui::AX_STATE_DEFAULT))
    msaa_state |= STATE_SYSTEM_DEFAULT;
  if (state.HasStateFlag(ui::AX_STATE_EXPANDED))
    msaa_state |= STATE_SYSTEM_EXPANDED;
  if (state.HasStateFlag(ui::AX_STATE_HASPOPUP))
    msaa_state |= STATE_SYSTEM_HASPOPUP;
  if (state.HasStateFlag(ui::AX_STATE_HOVERED))
    msaa_state |= STATE_SYSTEM_HOTTRACKED;
  if (state.HasStateFlag(ui::AX_STATE_INVISIBLE))
    msaa_state |= STATE_SYSTEM_INVISIBLE;
  if (state.HasStateFlag(ui::AX_STATE_LINKED))
    msaa_state |= STATE_SYSTEM_LINKED;
  if (state.HasStateFlag(ui::AX_STATE_OFFSCREEN))
    msaa_state |= STATE_SYSTEM_OFFSCREEN;
  if (state.HasStateFlag(ui::AX_STATE_PRESSED))
    msaa_state |= STATE_SYSTEM_PRESSED;
  if (state.HasStateFlag(ui::AX_STATE_PROTECTED))
    msaa_state |= STATE_SYSTEM_PROTECTED;
  if (state.HasStateFlag(ui::AX_STATE_READ_ONLY))
    msaa_state |= STATE_SYSTEM_READONLY;
  if (state.HasStateFlag(ui::AX_STATE_SELECTED))
    msaa_state |= STATE_SYSTEM_SELECTED;
  if (state.HasStateFlag(ui::AX_STATE_FOCUSED))
    msaa_state |= STATE_SYSTEM_FOCUSED;
  if (state.HasStateFlag(ui::AX_STATE_DISABLED))
    msaa_state |= STATE_SYSTEM_UNAVAILABLE;
  return msaa_state;
}

//
// Private methods.
//

bool NativeViewAccessibilityWin::IsNavDirNext(int nav_dir) const {
  return (nav_dir == NAVDIR_RIGHT ||
          nav_dir == NAVDIR_DOWN ||
          nav_dir == NAVDIR_NEXT);
}

bool NativeViewAccessibilityWin::IsValidNav(
    int nav_dir, int start_id, int lower_bound, int upper_bound) const {
  if (IsNavDirNext(nav_dir)) {
    if ((start_id + 1) > upper_bound) {
      return false;
    }
  } else {
    if ((start_id - 1) <= lower_bound) {
      return false;
    }
  }
  return true;
}

bool NativeViewAccessibilityWin::IsValidId(const VARIANT& child) const {
  // View accessibility returns an IAccessible for each view so we only support
  // the CHILDID_SELF id.
  return (VT_I4 == child.vt) && (CHILDID_SELF == child.lVal);
}

void NativeViewAccessibilityWin::SetState(
    VARIANT* msaa_state, View* view) {
  // Ensure the output param is initialized to zero.
  msaa_state->lVal = 0;

  // Default state; all views can have accessibility focus.
  msaa_state->lVal |= STATE_SYSTEM_FOCUSABLE;

  if (!view)
    return;

  if (!view->enabled())
    msaa_state->lVal |= STATE_SYSTEM_UNAVAILABLE;
  if (!view->visible())
    msaa_state->lVal |= STATE_SYSTEM_INVISIBLE;
  if (!strcmp(view->GetClassName(), CustomButton::kViewClassName)) {
    CustomButton* button = static_cast<CustomButton*>(view);
    if (button->IsHotTracked())
      msaa_state->lVal |= STATE_SYSTEM_HOTTRACKED;
  }
  if (view->HasFocus())
    msaa_state->lVal |= STATE_SYSTEM_FOCUSED;

  // Add on any view-specific states.
  ui::AXViewState view_state;
  view->GetAccessibleState(&view_state);
  msaa_state->lVal |= MSAAState(view_state);
}

base::string16 NativeViewAccessibilityWin::TextForIAccessibleText() {
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  if (state.role == ui::AX_ROLE_TEXT_FIELD)
    return state.value;
  else
    return state.name;
}

void NativeViewAccessibilityWin::HandleSpecialTextOffset(
    const base::string16& text, LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH) {
    *offset = static_cast<LONG>(text.size());
  } else if (*offset == IA2_TEXT_OFFSET_CARET) {
    get_caretOffset(offset);
  }
}

ui::TextBoundaryType NativeViewAccessibilityWin::IA2TextBoundaryToTextBoundary(
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

LONG NativeViewAccessibilityWin::FindBoundary(
    const base::string16& text,
    IA2TextBoundaryType ia2_boundary,
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  HandleSpecialTextOffset(text, &start_offset);
  ui::TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
  std::vector<int32> line_breaks;
  return ui::FindAccessibleTextBoundary(
      text, line_breaks, boundary, start_offset, direction);
}

void NativeViewAccessibilityWin::PopulateChildWidgetVector(
    std::vector<Widget*>* result_child_widgets) {
  const Widget* widget = view()->GetWidget();
  if (!widget)
    return;

  std::set<Widget*> child_widgets;
  Widget::GetAllChildWidgets(widget->GetNativeView(), &child_widgets);
  Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
  for (std::set<Widget*>::const_iterator iter = child_widgets.begin();
           iter != child_widgets.end(); ++iter) {
    Widget* child_widget = *iter;
    if (child_widget == widget)
      continue;

    if (!child_widget->IsVisible())
      continue;

    if (widget->GetNativeWindowProperty(kWidgetNativeViewHostKey))
      continue;

    result_child_widgets->push_back(child_widget);
  }
}

void NativeViewAccessibilityWin::AddAlertTarget() {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  for (size_t i = 0; i < alert_target_view_storage_ids_.size(); ++i) {
    int view_storage_id = alert_target_view_storage_ids_[i];
    View* view = view_storage->RetrieveView(view_storage_id);
    if (view == view_)
      return;
  }
  int view_storage_id = view_storage->CreateStorageID();
  view_storage->StoreView(view_storage_id, view_);
  alert_target_view_storage_ids_.push_back(view_storage_id);
}

void NativeViewAccessibilityWin::RemoveAlertTarget() {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  size_t i = 0;
  while (i < alert_target_view_storage_ids_.size()) {
    int view_storage_id = alert_target_view_storage_ids_[i];
    View* view = view_storage->RetrieveView(view_storage_id);
    if (view == NULL || view == view_) {
      alert_target_view_storage_ids_.erase(
          alert_target_view_storage_ids_.begin() + i);
    } else {
      ++i;
    }
  }
}

}  // namespace views
