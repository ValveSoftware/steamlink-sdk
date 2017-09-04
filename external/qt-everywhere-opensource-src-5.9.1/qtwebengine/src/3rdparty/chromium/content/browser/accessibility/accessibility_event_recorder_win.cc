// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include <oleacc.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"

namespace content {

namespace {

std::string RoleVariantToString(const base::win::ScopedVariant& role) {
  if (role.type() == VT_I4) {
    return base::UTF16ToUTF8(IAccessibleRoleToString(V_I4(role.ptr())));
  } else if (role.type() == VT_BSTR) {
    return base::UTF16ToUTF8(
        base::string16(V_BSTR(role.ptr()), SysStringLen(V_BSTR(role.ptr()))));
  }
  return std::string();
}

HRESULT QueryIAccessible2(IAccessible* accessible, IAccessible2** accessible2) {
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.Receive());
  return SUCCEEDED(hr) ?
      service_provider->QueryService(IID_IAccessible2, accessible2) : hr;
}

HRESULT QueryIAccessibleText(IAccessible* accessible,
                             IAccessibleText** accessible_text) {
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.Receive());
  return SUCCEEDED(hr) ?
      service_provider->QueryService(IID_IAccessibleText, accessible_text) : hr;
}

std::string BstrToUTF8(BSTR bstr) {
  base::string16 str16(bstr, SysStringLen(bstr));

  // IAccessibleText returns the text you get by appending all static text
  // children, with an "embedded object character" for each non-text child.
  // Pretty-print the embedded object character as <obj> so that test output
  // is human-readable.
  base::StringPiece16 embedded_character(
      &BrowserAccessibilityWin::kEmbeddedCharacter, 1);
  base::ReplaceChars(str16, embedded_character, L"<obj>", &str16);

  return base::UTF16ToUTF8(str16);
}

std::string AccessibilityEventToStringUTF8(int32_t event_id) {
  return base::UTF16ToUTF8(AccessibilityEventToString(event_id));
}

}  // namespace

class AccessibilityEventRecorderWin : public AccessibilityEventRecorder {
 public:
  explicit AccessibilityEventRecorderWin(BrowserAccessibilityManager* manager);
  ~AccessibilityEventRecorderWin() override;

  // Callback registered by SetWinEventHook. Just calls OnWinEventHook.
  static void CALLBACK WinEventHookThunk(
      HWINEVENTHOOK handle,
      DWORD event,
      HWND hwnd,
      LONG obj_id,
      LONG child_id,
      DWORD event_thread,
      DWORD event_time);

 private:
  // Called by the thunk registered by SetWinEventHook. Retrives accessibility
  // info about the node the event was fired on and appends a string to
  // the event log.
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  // Wrapper around AccessibleObjectFromWindow because the function call
  // inexplicably flakes sometimes on build/trybots.
  HRESULT AccessibleObjectFromWindowWrapper(
      HWND hwnd, DWORD dwId, REFIID riid, void **ppvObject);

  HWINEVENTHOOK win_event_hook_handle_;
  static AccessibilityEventRecorderWin* instance_;
};

// static
AccessibilityEventRecorderWin*
AccessibilityEventRecorderWin::instance_ = nullptr;

// static
AccessibilityEventRecorder* AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager) {
  return new AccessibilityEventRecorderWin(manager);
}

// static
void CALLBACK AccessibilityEventRecorderWin::WinEventHookThunk(
    HWINEVENTHOOK handle,
    DWORD event,
    HWND hwnd,
    LONG obj_id,
    LONG child_id,
    DWORD event_thread,
    DWORD event_time) {
  if (instance_) {
    instance_->OnWinEventHook(handle, event, hwnd, obj_id, child_id,
                              event_thread, event_time);
  }
}

AccessibilityEventRecorderWin::AccessibilityEventRecorderWin(
    BrowserAccessibilityManager* manager)
    : AccessibilityEventRecorder(manager) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " WinAccessibilityEventMonitor at a time.";
  instance_ = this;
  win_event_hook_handle_ = SetWinEventHook(
      EVENT_MIN,
      EVENT_MAX,
      GetModuleHandle(NULL),
      &AccessibilityEventRecorderWin::WinEventHookThunk,
      GetCurrentProcessId(),
      0,  // Hook all threads
      WINEVENT_INCONTEXT);
  CHECK(win_event_hook_handle_);
}

AccessibilityEventRecorderWin::~AccessibilityEventRecorderWin() {
  UnhookWinEvent(win_event_hook_handle_);
  instance_ = NULL;
}

void AccessibilityEventRecorderWin::OnWinEventHook(
    HWINEVENTHOOK handle,
    DWORD event,
    HWND hwnd,
    LONG obj_id,
    LONG child_id,
    DWORD event_thread,
    DWORD event_time) {
  base::win::ScopedComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindowWrapper(
      hwnd,
      obj_id,
      IID_IAccessible,
      reinterpret_cast<void**>(browser_accessible.Receive()));
  if (!SUCCEEDED(hr)) {
    // Note: our event hook will pick up some superfluous events we
    // don't care about, so it's safe to just ignore these failures.
    // Same below for other HRESULT checks.
    VLOG(1) << "Ignoring result " << hr << " from AccessibleObjectFromWindow";
    return;
  }

  base::win::ScopedVariant childid_variant(child_id);
  base::win::ScopedComPtr<IDispatch> dispatch;
  hr = browser_accessible->get_accChild(childid_variant, dispatch.Receive());
  if (!SUCCEEDED(hr) || !dispatch) {
    VLOG(1) << "Ignoring result " << hr << " and result " << dispatch
            << " from get_accChild";
    return;
  }

  base::win::ScopedComPtr<IAccessible> iaccessible;
  hr = dispatch.QueryInterface(iaccessible.Receive());
  if (!SUCCEEDED(hr)) {
    VLOG(1) << "Ignoring result " << hr << " from QueryInterface";
    return;
  }

  std::string event_str = AccessibilityEventToStringUTF8(event);
  if (event_str.empty()) {
    VLOG(1) << "Ignoring event " << event;
    return;
  }

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedVariant role;
  iaccessible->get_accRole(childid_self, role.Receive());
  base::win::ScopedBstr name_bstr;
  iaccessible->get_accName(childid_self, name_bstr.Receive());
  base::win::ScopedBstr value_bstr;
  iaccessible->get_accValue(childid_self, value_bstr.Receive());
  base::win::ScopedVariant state;
  iaccessible->get_accState(childid_self, state.Receive());
  int ia_state = V_I4(state.ptr());

  // Avoid flakiness. Events fired on a WINDOW are out of the control
  // of a test.
  if (role.type() == VT_I4 && ROLE_SYSTEM_WINDOW == V_I4(role.ptr())) {
    VLOG(1) << "Ignoring event " << event << " on ROLE_SYSTEM_WINDOW";
    return;
  }

  // Avoid flakiness. The "offscreen" state depends on whether the browser
  // window is frontmost or not, and "hottracked" depends on whether the
  // mouse cursor happens to be over the element.
  ia_state &= (~STATE_SYSTEM_OFFSCREEN & ~STATE_SYSTEM_HOTTRACKED);

  // The "readonly" state is set on almost every node and doesn't typically
  // change, so filter it out to keep the output less verbose.
  ia_state &= ~STATE_SYSTEM_READONLY;

  AccessibleStates ia2_state = 0;
  base::win::ScopedComPtr<IAccessible2> iaccessible2;
  hr = QueryIAccessible2(iaccessible.get(), iaccessible2.Receive());
  if (SUCCEEDED(hr))
    iaccessible2->get_states(&ia2_state);

  std::string log = base::StringPrintf(
      "%s on role=%s", event_str.c_str(), RoleVariantToString(role).c_str());
  if (name_bstr.Length() > 0)
    log += base::StringPrintf(" name=\"%s\"", BstrToUTF8(name_bstr).c_str());
  if (value_bstr.Length() > 0)
    log += base::StringPrintf(" value=\"%s\"", BstrToUTF8(value_bstr).c_str());
  log += " ";
  log += base::UTF16ToUTF8(IAccessibleStateToString(ia_state));
  log += " ";
  log += base::UTF16ToUTF8(IAccessible2StateToString(ia2_state));

  // For TEXT_REMOVED and TEXT_INSERTED events, query the text that was
  // inserted or removed and include that in the log.
  base::win::ScopedComPtr<IAccessibleText> accessible_text;
  hr = QueryIAccessibleText(iaccessible.get(), accessible_text.Receive());
  if (SUCCEEDED(hr)) {
    if (event == IA2_EVENT_TEXT_REMOVED) {
      IA2TextSegment old_text;
      if (SUCCEEDED(accessible_text->get_oldText(&old_text))) {
        log += base::StringPrintf(" old_text={'%s' start=%d end=%d}",
                                  BstrToUTF8(old_text.text).c_str(),
                                  old_text.start,
                                  old_text.end);
      }
    }
    if (event == IA2_EVENT_TEXT_INSERTED) {
      IA2TextSegment new_text;
      if (SUCCEEDED(accessible_text->get_newText(&new_text))) {
        log += base::StringPrintf(" new_text={'%s' start=%d end=%d}",
                                  BstrToUTF8(new_text.text).c_str(),
                                  new_text.start,
                                  new_text.end);
      }
    }
  }

  log = base::UTF16ToUTF8(
      base::CollapseWhitespace(base::UTF8ToUTF16(log), true));
  event_logs_.push_back(log);
}

HRESULT AccessibilityEventRecorderWin::AccessibleObjectFromWindowWrapper(
    HWND hwnd, DWORD dw_id, REFIID riid, void** ppv_object) {
  HRESULT hr = ::AccessibleObjectFromWindow(hwnd, dw_id, riid, ppv_object);
  if (SUCCEEDED(hr))
    return hr;

  // The above call to ::AccessibleObjectFromWindow fails for unknown
  // reasons every once in a while on the bots.  Work around it by grabbing
  // the object directly from the BrowserAccessibilityManager.
  HWND accessibility_hwnd =
      manager_->delegate()->AccessibilityGetAcceleratedWidget();
  if (accessibility_hwnd != hwnd)
    return E_FAIL;

  IAccessible* obj = ToBrowserAccessibilityWin(manager_->GetRoot());
  obj->AddRef();
  *ppv_object = obj;
  return S_OK;
}

}  // namespace content
