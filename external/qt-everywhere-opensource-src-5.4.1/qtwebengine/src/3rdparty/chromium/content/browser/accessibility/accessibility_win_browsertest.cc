// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {

namespace {

// Helpers --------------------------------------------------------------------

base::win::ScopedComPtr<IAccessible> GetAccessibleFromResultVariant(
    IAccessible* parent,
    VARIANT* var) {
  base::win::ScopedComPtr<IAccessible> ptr;
  switch (V_VT(var)) {
    case VT_DISPATCH: {
      IDispatch* dispatch = V_DISPATCH(var);
      if (dispatch)
        ptr.QueryFrom(dispatch);
      break;
    }

    case VT_I4: {
      base::win::ScopedComPtr<IDispatch> dispatch;
      HRESULT hr = parent->get_accChild(*var, dispatch.Receive());
      EXPECT_TRUE(SUCCEEDED(hr));
      if (dispatch)
        dispatch.QueryInterface(ptr.Receive());
      break;
    }
  }
  return ptr;
}

HRESULT QueryIAccessible2(IAccessible* accessible, IAccessible2** accessible2) {
  // TODO(ctguil): For some reason querying the IAccessible2 interface from
  // IAccessible fails.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.Receive());
  return SUCCEEDED(hr) ?
      service_provider->QueryService(IID_IAccessible2, accessible2) : hr;
}

// Recursively search through all of the descendants reachable from an
// IAccessible node and return true if we find one with the given role
// and name.
void RecursiveFindNodeInAccessibilityTree(IAccessible* node,
                                          int32 expected_role,
                                          const std::wstring& expected_name,
                                          int32 depth,
                                          bool* found) {
  base::win::ScopedBstr name_bstr;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  node->get_accName(childid_self, name_bstr.Receive());
  std::wstring name(name_bstr, name_bstr.Length());
  base::win::ScopedVariant role;
  node->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(VT_I4, role.type());

  // Print the accessibility tree as we go, because if this test fails
  // on the bots, this is really helpful in figuring out why.
  for (int i = 0; i < depth; i++)
    printf("  ");
  printf("role=%s name=%s\n",
      base::WideToUTF8(IAccessibleRoleToString(V_I4(&role))).c_str(),
      base::WideToUTF8(name).c_str());

  if (expected_role == V_I4(&role) && expected_name == name) {
    *found = true;
    return;
  }

  LONG child_count = 0;
  HRESULT hr = node->get_accChildCount(&child_count);
  ASSERT_EQ(S_OK, hr);

  scoped_ptr<VARIANT[]> child_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(
      node, 0, child_count, child_array.get(), &obtained_count);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, obtained_count);

  for (int index = 0; index < obtained_count; index++) {
    base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromResultVariant(node, &child_array.get()[index]));
    if (child_accessible) {
      RecursiveFindNodeInAccessibilityTree(
          child_accessible.get(), expected_role, expected_name, depth + 1,
          found);
      if (*found)
        return;
    }
  }
}


// AccessibilityWinBrowserTest ------------------------------------------------

class AccessibilityWinBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityWinBrowserTest();
  virtual ~AccessibilityWinBrowserTest();

 protected:
  void LoadInitialAccessibilityTreeFromHtml(const std::string& html);
  IAccessible* GetRendererAccessible();
  void ExecuteScript(const std::wstring& script);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityWinBrowserTest);
};

AccessibilityWinBrowserTest::AccessibilityWinBrowserTest() {
}

AccessibilityWinBrowserTest::~AccessibilityWinBrowserTest() {
}

void AccessibilityWinBrowserTest::LoadInitialAccessibilityTreeFromHtml(
    const std::string& html) {
  AccessibilityNotificationWaiter waiter(
      shell(), AccessibilityModeComplete,
      ui::AX_EVENT_LOAD_COMPLETE);
  GURL html_data_url("data:text/html," + html);
  NavigateToURL(shell(), html_data_url);
  waiter.WaitForNotification();
}

// Retrieve the MSAA client accessibility object for the Render Widget Host View
// of the selected tab.
IAccessible* AccessibilityWinBrowserTest::GetRendererAccessible() {
  content::WebContents* web_contents = shell()->web_contents();
  return web_contents->GetRenderWidgetHostView()->GetNativeViewAccessible();
}

void AccessibilityWinBrowserTest::ExecuteScript(const std::wstring& script) {
  shell()->web_contents()->GetMainFrame()->ExecuteJavaScript(script);
}


// AccessibleChecker ----------------------------------------------------------

class AccessibleChecker {
 public:
  // This constructor can be used if the IA2 role will be the same as the MSAA
  // role.
  AccessibleChecker(const std::wstring& expected_name,
                    int32 expected_role,
                    const std::wstring& expected_value);
  AccessibleChecker(const std::wstring& expected_name,
                    int32 expected_role,
                    int32 expected_ia2_role,
                    const std::wstring& expected_value);
  AccessibleChecker(const std::wstring& expected_name,
                    const std::wstring& expected_role,
                    int32 expected_ia2_role,
                    const std::wstring& expected_value);

  // Append an AccessibleChecker that verifies accessibility information for
  // a child IAccessible. Order is important.
  void AppendExpectedChild(AccessibleChecker* expected_child);

  // Check that the name and role of the given IAccessible instance and its
  // descendants match the expected names and roles that this object was
  // initialized with.
  void CheckAccessible(IAccessible* accessible);

  // Set the expected value for this AccessibleChecker.
  void SetExpectedValue(const std::wstring& expected_value);

  // Set the expected state for this AccessibleChecker.
  void SetExpectedState(LONG expected_state);

 private:
  typedef std::vector<AccessibleChecker*> AccessibleCheckerVector;

  void CheckAccessibleName(IAccessible* accessible);
  void CheckAccessibleRole(IAccessible* accessible);
  void CheckIA2Role(IAccessible* accessible);
  void CheckAccessibleValue(IAccessible* accessible);
  void CheckAccessibleState(IAccessible* accessible);
  void CheckAccessibleChildren(IAccessible* accessible);
  base::string16 RoleVariantToString(const base::win::ScopedVariant& role);

  // Expected accessible name. Checked against IAccessible::get_accName.
  std::wstring name_;

  // Expected accessible role. Checked against IAccessible::get_accRole.
  base::win::ScopedVariant role_;

  // Expected IAccessible2 role. Checked against IAccessible2::role.
  int32 ia2_role_;

  // Expected accessible value. Checked against IAccessible::get_accValue.
  std::wstring value_;

  // Expected accessible state. Checked against IAccessible::get_accState.
  LONG state_;

  // Expected accessible children. Checked using IAccessible::get_accChildCount
  // and ::AccessibleChildren.
  AccessibleCheckerVector children_;
};

AccessibleChecker::AccessibleChecker(const std::wstring& expected_name,
                                     int32 expected_role,
                                     const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_role),
      value_(expected_value),
      state_(-1) {
}

AccessibleChecker::AccessibleChecker(const std::wstring& expected_name,
                                     int32 expected_role,
                                     int32 expected_ia2_role,
                                     const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_ia2_role),
      value_(expected_value),
      state_(-1) {
}

AccessibleChecker::AccessibleChecker(const std::wstring& expected_name,
                                     const std::wstring& expected_role,
                                     int32 expected_ia2_role,
                                     const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role.c_str()),
      ia2_role_(expected_ia2_role),
      value_(expected_value),
      state_(-1) {
}

void AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibleChecker::CheckAccessible(IAccessible* accessible) {
  SCOPED_TRACE("while checking " +
                   base::UTF16ToUTF8(RoleVariantToString(role_)));
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckIA2Role(accessible);
  CheckAccessibleValue(accessible);
  CheckAccessibleState(accessible);
  CheckAccessibleChildren(accessible);
}

void AccessibleChecker::SetExpectedValue(const std::wstring& expected_value) {
  value_ = expected_value;
}

void AccessibleChecker::SetExpectedState(LONG expected_state) {
  state_ = expected_state;
}

void AccessibleChecker::CheckAccessibleName(IAccessible* accessible) {
  base::win::ScopedBstr name;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accName(childid_self, name.Receive());

  if (name_.empty()) {
    // If the object doesn't have name S_FALSE should be returned.
    EXPECT_EQ(S_FALSE, hr);
  } else {
    // Test that the correct string was returned.
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(name_, std::wstring(name, name.Length()));
  }
}

void AccessibleChecker::CheckAccessibleRole(IAccessible* accessible) {
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(0, role_.Compare(role))
      << "Expected role: " << RoleVariantToString(role_)
      << "\nGot role: " << RoleVariantToString(role);
}

void AccessibleChecker::CheckIA2Role(IAccessible* accessible) {
  base::win::ScopedComPtr<IAccessible2> accessible2;
  HRESULT hr = QueryIAccessible2(accessible, accessible2.Receive());
  ASSERT_EQ(S_OK, hr);
  long ia2_role = 0;
  hr = accessible2->role(&ia2_role);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(ia2_role_, ia2_role)
    << "Expected ia2 role: " << IAccessible2RoleToString(ia2_role_)
    << "\nGot ia2 role: " << IAccessible2RoleToString(ia2_role);
}

void AccessibleChecker::CheckAccessibleValue(IAccessible* accessible) {
  // Don't check the value if if's a DOCUMENT role, because the value
  // is supposed to be the url (and we don't keep track of that in the
  // test expectations).
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);
  if (role.type() == VT_I4 && V_I4(&role) == ROLE_SYSTEM_DOCUMENT)
    return;

  // Get the value.
  base::win::ScopedBstr value;
  hr = accessible->get_accValue(childid_self, value.Receive());
  EXPECT_EQ(S_OK, hr);

  // Test that the correct string was returned.
  EXPECT_EQ(value_, std::wstring(value, value.Length()));
}

void AccessibleChecker::CheckAccessibleState(IAccessible* accessible) {
  if (state_ < 0)
    return;

  base::win::ScopedVariant state;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accState(childid_self, state.Receive());
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(VT_I4, state.type());
  LONG obj_state = V_I4(&state);
  // Avoid flakiness. The "offscreen" state depends on whether the browser
  // window is frontmost or not, and "hottracked" depends on whether the
  // mouse cursor happens to be over the element.
  obj_state &= ~(STATE_SYSTEM_OFFSCREEN | STATE_SYSTEM_HOTTRACKED);
  EXPECT_EQ(state_, obj_state)
    << "Expected state: " << IAccessibleStateToString(state_)
    << "\nGot state: " << IAccessibleStateToString(obj_state);
}

void AccessibleChecker::CheckAccessibleChildren(IAccessible* parent) {
  LONG child_count = 0;
  HRESULT hr = parent->get_accChildCount(&child_count);
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, children_.size());

  scoped_ptr<VARIANT[]> child_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(parent, 0, child_count,
                          child_array.get(), &obtained_count);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, obtained_count);

  VARIANT* child = child_array.get();
  for (AccessibleCheckerVector::iterator child_checker = children_.begin();
       child_checker != children_.end();
       ++child_checker, ++child) {
    base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromResultVariant(parent, child));
    ASSERT_TRUE(child_accessible.get());
    (*child_checker)->CheckAccessible(child_accessible);
  }
}

base::string16 AccessibleChecker::RoleVariantToString(
    const base::win::ScopedVariant& role) {
  if (role.type() == VT_I4)
    return IAccessibleRoleToString(V_I4(&role));
  if (role.type() == VT_BSTR)
    return base::string16(V_BSTR(&role), SysStringLen(V_BSTR(&role)));
  return base::string16();
}

}  // namespace


// Tests ----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestBusyAccessibilityTree) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // The initial accessible returned should have state STATE_SYSTEM_BUSY while
  // the accessibility tree is being requested from the renderer.
  AccessibleChecker document1_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                      std::wstring());
  document1_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED |
      STATE_SYSTEM_BUSY);
  document1_checker.CheckAccessible(GetRendererAccessible());
}

// Periodically failing.  See crbug.com/145537
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestNotificationActiveDescendantChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<ul tabindex='-1' role='radiogroup' aria-label='ul'>"
      "<li id='li'>li</li></ul>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker list_marker_checker(L"\x2022", ROLE_SYSTEM_TEXT,
                                        std::wstring());
  AccessibleChecker static_text_checker(L"li", ROLE_SYSTEM_TEXT,
                                        std::wstring());
  AccessibleChecker list_item_checker(std::wstring(), ROLE_SYSTEM_LISTITEM,
                                      std::wstring());
  list_item_checker.SetExpectedState(STATE_SYSTEM_READONLY);
  AccessibleChecker radio_group_checker(L"ul", ROLE_SYSTEM_GROUPING,
                                        IA2_ROLE_SECTION, std::wstring());
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  list_item_checker.AppendExpectedChild(&list_marker_checker);
  list_item_checker.AppendExpectedChild(&static_text_checker);
  radio_group_checker.AppendExpectedChild(&list_item_checker);
  document_checker.AppendExpectedChild(&radio_group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set focus to the radio group.
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_FOCUS));
  ExecuteScript(L"document.body.children[0].focus()");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the active descendant of the radio group
  waiter.reset(new AccessibilityNotificationWaiter(
      shell(), AccessibilityModeComplete,
      ui::AX_EVENT_FOCUS));
  ExecuteScript(
      L"document.body.children[0].setAttribute('aria-activedescendant', 'li')");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  list_item_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED);
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationCheckedStateChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox' /></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker checkbox_checker(std::wstring(), ROLE_SYSTEM_CHECKBUTTON,
                                     std::wstring());
  checkbox_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(std::wstring(), L"body", IA2_ROLE_SECTION,
                                 std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  body_checker.AppendExpectedChild(&checkbox_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Check the checkbox.
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_CHECKED_STATE_CHANGED));
  ExecuteScript(L"document.body.children[0].checked=true");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  checkbox_checker.SetExpectedState(
      STATE_SYSTEM_CHECKED | STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml("<body role=group></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(),
          AccessibilityModeComplete,
          ui::AX_EVENT_CHILDREN_CHANGED));
  ExecuteScript(L"document.body.innerHTML='<b>new text</b>'");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker text_checker(
      L"new text", ROLE_SYSTEM_STATICTEXT, std::wstring());
  group_checker.AppendExpectedChild(&text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged2) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml(
      "<div role=group style='visibility: hidden'>text</div>");

  // Check the accessible tree of the browser.
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_CHILDREN_CHANGED));
  ExecuteScript(L"document.body.children[0].style.visibility='visible'");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker static_text_checker(L"text", ROLE_SYSTEM_STATICTEXT,
                                        std::wstring());
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  group_checker.AppendExpectedChild(&static_text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationFocusChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml("<div role=group tabindex='-1'></div>");

  // Check the browser's copy of the renderer accessibility tree.
  SCOPED_TRACE("Check initial tree");
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the div in the document
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_FOCUS));
  ExecuteScript(L"document.body.children[0].focus()");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing div");
  group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the document accessible. This will un-focus the current node.
  waiter.reset(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_BLUR));
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = document_accessible->accSelect(SELFLAG_TAKEFOCUS, childid_self);
  ASSERT_EQ(S_OK, hr);
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing document again");
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationValueChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='text' value='old value'/></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker text_field_checker(std::wstring(), ROLE_SYSTEM_TEXT,
                                       L"old value");
  text_field_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(std::wstring(), L"body", IA2_ROLE_SECTION,
                                 std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  body_checker.AppendExpectedChild(&text_field_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the value of the text control
  scoped_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(
          shell(), AccessibilityModeComplete,
          ui::AX_EVENT_VALUE_CHANGED));
  ExecuteScript(L"document.body.children[0].value='new value'");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  text_field_checker.SetExpectedValue(L"new value");
  document_checker.CheckAccessible(GetRendererAccessible());
}

// This test verifies that the web content's accessibility tree is a
// descendant of the main browser window's accessibility tree, so that
// tools like AccExplorer32 or AccProbe can be used to examine Chrome's
// accessibility support.
//
// If you made a change and this test now fails, check that the NativeViewHost
// that wraps the tab contents returns the IAccessible implementation
// provided by RenderWidgetHostViewWin in GetNativeViewAccessible().
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       ContainsRendererAccessibilityTree) {
  LoadInitialAccessibilityTreeFromHtml(
      "<html><head><title>MyDocument</title></head>"
      "<body>Content</body></html>");

  // Get the accessibility object for the window tree host.
  aura::Window* window = shell()->window();
  CHECK(window);
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  CHECK(window_tree_host);
  HWND hwnd = window_tree_host->GetAcceleratedWidget();
  CHECK(hwnd);
  base::win::ScopedComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindow(
      hwnd,
      OBJID_WINDOW,
      IID_IAccessible,
      reinterpret_cast<void**>(browser_accessible.Receive()));
  ASSERT_EQ(S_OK, hr);

  bool found = false;
  RecursiveFindNodeInAccessibilityTree(
      browser_accessible.get(), ROLE_SYSTEM_DOCUMENT, L"MyDocument", 0, &found);
  ASSERT_EQ(found, true);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SupportsISimpleDOM) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox' /></body>");

  // Get the IAccessible object for the document.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));

  // Get the ISimpleDOM object for the document.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = static_cast<IAccessible*>(document_accessible)->QueryInterface(
      service_provider.Receive());
  ASSERT_EQ(S_OK, hr);
  const GUID refguid = {0x0c539790, 0x12e4, 0x11cf,
                        0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8};
  base::win::ScopedComPtr<ISimpleDOMNode> document_isimpledomnode;
  hr = static_cast<IServiceProvider *>(service_provider)->QueryService(
      refguid, IID_ISimpleDOMNode,
      reinterpret_cast<void**>(document_isimpledomnode.Receive()));
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedBstr node_name;
  short name_space_id;  // NOLINT
  base::win::ScopedBstr node_value;
  unsigned int num_children;
  unsigned int unique_id;
  unsigned short node_type;  // NOLINT
  hr = document_isimpledomnode->get_nodeInfo(
      node_name.Receive(), &name_space_id, node_value.Receive(), &num_children,
      &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(NODETYPE_DOCUMENT, node_type);
  EXPECT_EQ(1, num_children);
  node_name.Reset();
  node_value.Reset();

  base::win::ScopedComPtr<ISimpleDOMNode> body_isimpledomnode;
  hr = document_isimpledomnode->get_firstChild(
      body_isimpledomnode.Receive());
  ASSERT_EQ(S_OK, hr);
  hr = body_isimpledomnode->get_nodeInfo(
      node_name.Receive(), &name_space_id, node_value.Receive(), &num_children,
      &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"body", std::wstring(node_name, node_name.Length()));
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(1, num_children);
  node_name.Reset();
  node_value.Reset();

  base::win::ScopedComPtr<ISimpleDOMNode> checkbox_isimpledomnode;
  hr = body_isimpledomnode->get_firstChild(
      checkbox_isimpledomnode.Receive());
  ASSERT_EQ(S_OK, hr);
  hr = checkbox_isimpledomnode->get_nodeInfo(
      node_name.Receive(), &name_space_id, node_value.Receive(), &num_children,
      &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"input", std::wstring(node_name, node_name.Length()));
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(0, num_children);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestRoleGroup) {
  LoadInitialAccessibilityTreeFromHtml(
      "<fieldset></fieldset><div role=group></div>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker grouping1_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                      std::wstring());
  AccessibleChecker grouping2_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                      std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&grouping1_checker);
  document_checker.AppendExpectedChild(&grouping2_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

}  // namespace content
