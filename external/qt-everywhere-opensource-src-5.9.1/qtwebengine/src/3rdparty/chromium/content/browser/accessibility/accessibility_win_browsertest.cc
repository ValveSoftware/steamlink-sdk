// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
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
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {

namespace {


const char INPUT_CONTENTS[] = "Moz/5.0 (ST 6.x; WWW33) "
    "WebKit  \"KHTML, like\".";
const char TEXTAREA_CONTENTS[] = "Moz/5.0 (ST 6.x; WWW33)\n"
    "WebKit \n\"KHTML, like\".";
const LONG CONTENTS_LENGTH = static_cast<LONG>(
    (sizeof(INPUT_CONTENTS) - 1) / sizeof(char));


// AccessibilityWinBrowserTest ------------------------------------------------

class AccessibilityWinBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityWinBrowserTest();
  ~AccessibilityWinBrowserTest() override;

 protected:
  class AccessibleChecker;
  void LoadInitialAccessibilityTreeFromHtml(const std::string& html);
  IAccessible* GetRendererAccessible();
  void ExecuteScript(const std::wstring& script);
  void SetUpInputField(
      base::win::ScopedComPtr<IAccessibleText>* input_text);
  void SetUpTextareaField(
      base::win::ScopedComPtr<IAccessibleText>* textarea_text);
  void SetUpSampleParagraph(
      base::win::ScopedComPtr<IAccessibleText>* paragraph_text);

  static base::win::ScopedComPtr<IAccessible> GetAccessibleFromVariant(
      IAccessible* parent,
      VARIANT* var);
  static HRESULT QueryIAccessible2(IAccessible* accessible,
                                   IAccessible2** accessible2);
  static void FindNodeInAccessibilityTree(IAccessible* node,
                                          int32_t expected_role,
                                          const std::wstring& expected_name,
                                          int32_t depth,
                                          bool* found);
  static void CheckTextAtOffset(
      base::win::ScopedComPtr<IAccessibleText>& object,
      LONG offset,
      IA2TextBoundaryType boundary_type,
      LONG expected_start_offset,
      LONG expected_end_offset,
      const std::wstring& expected_text);
  static std::vector<base::win::ScopedVariant> GetAllAccessibleChildren(
      IAccessible* element);

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
      shell()->web_contents(), AccessibilityModeComplete,
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
  shell()->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(script);
}

// Loads a page with  an input text field and places sample text in it. Also,
// places the caret on the last character.
void AccessibilityWinBrowserTest::SetUpInputField(
    base::win::ScopedComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInitialAccessibilityTreeFromHtml(std::string("<!DOCTYPE html><html><body>"
      "<form><label for='textField'>Browser name:</label>"
      "<input type='text' id='textField' name='name' value='") +
      net::EscapeQueryParamValue(INPUT_CONTENTS, false) + std::string(
      "'></form></body></html>"));

  // Retrieve the IAccessible interface for the web page.
  base::win::ScopedComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.get());
  ASSERT_EQ(1u, document_children.size());

  base::win::ScopedComPtr<IAccessible2> form;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.get(), document_children[0].AsInput())
          .get(),
      form.Receive()));
  std::vector<base::win::ScopedVariant> form_children =
      GetAllAccessibleChildren(form.get());
  ASSERT_EQ(2u, form_children.size());

  // Find the input text field.
  base::win::ScopedComPtr<IAccessible2> input;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(form.get(), form_children[1].AsInput()).get(),
      input.Receive()));
  LONG input_role = 0;
  ASSERT_HRESULT_SUCCEEDED(input->role(&input_role));
  ASSERT_EQ(ROLE_SYSTEM_TEXT, input_role);

  // Retrieve the IAccessibleText interface for the field.
  ASSERT_HRESULT_SUCCEEDED(input.QueryInterface(input_text->Receive()));

  // Set the caret on the last character.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), AccessibilityModeComplete,
      ui::AX_EVENT_TEXT_SELECTION_CHANGED);
  std::wstring caret_offset = base::UTF16ToWide(base::IntToString16(
      static_cast<int>(CONTENTS_LENGTH - 1)));
  ExecuteScript(std::wstring(
      L"var textField = document.getElementById('textField');"
      L"textField.focus();"
      L"textField.setSelectionRange(") +
      caret_offset + L"," + caret_offset + L");");
  waiter.WaitForNotification();
}

// Loads a page with  a textarea text field and places sample text in it. Also,
// places the caret on the last character.
void AccessibilityWinBrowserTest::SetUpTextareaField(
    base::win::ScopedComPtr<IAccessibleText>* textarea_text) {
  ASSERT_NE(nullptr, textarea_text);
  LoadInitialAccessibilityTreeFromHtml(std::string("<!DOCTYPE html><html><body>"
      "<textarea id='textField' rows='3' cols='60'>") +
      net::EscapeQueryParamValue(TEXTAREA_CONTENTS, false) + std::string(
      "</textarea></body></html>"));

  // Retrieve the IAccessible interface for the web page.
  base::win::ScopedComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.get());
  ASSERT_EQ(1u, document_children.size());

  base::win::ScopedComPtr<IAccessible2> section;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.get(), document_children[0].AsInput())
          .get(),
      section.Receive()));
  std::vector<base::win::ScopedVariant> section_children =
      GetAllAccessibleChildren(section.get());
  ASSERT_EQ(1u, section_children.size());

  // Find the textarea text field.
  base::win::ScopedComPtr<IAccessible2> textarea;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(section.get(), section_children[0].AsInput())
          .get(),
      textarea.Receive()));
  LONG textarea_role = 0;
  ASSERT_HRESULT_SUCCEEDED(textarea->role(&textarea_role));
  ASSERT_EQ(ROLE_SYSTEM_TEXT, textarea_role);

  // Retrieve the IAccessibleText interface for the field.
  ASSERT_HRESULT_SUCCEEDED(textarea.QueryInterface(textarea_text->Receive()));

  // Set the caret on the last character.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), AccessibilityModeComplete,
      ui::AX_EVENT_TEXT_SELECTION_CHANGED);
  std::wstring caret_offset = base::UTF16ToWide(base::IntToString16(
      static_cast<int>(CONTENTS_LENGTH - 1)));
  ExecuteScript(std::wstring(
      L"var textField = document.getElementById('textField');"
      L"textField.focus();"
      L"textField.setSelectionRange(") +
      caret_offset + L"," + caret_offset + L");");
  waiter.WaitForNotification();
}

// Loads a page with  a paragraph of sample text.
void AccessibilityWinBrowserTest::SetUpSampleParagraph(
    base::win::ScopedComPtr<IAccessibleText>* paragraph_text) {
  ASSERT_NE(nullptr, paragraph_text);
  LoadInitialAccessibilityTreeFromHtml(
      "<!DOCTYPE html><html><body>"
      "<p><b>Game theory</b> is \"the study of "
      "<a href=\"#\" title=\"Mathematical model\">mathematical models</a> "
      "of conflict and<br>cooperation between intelligent rational "
      "decision-makers.\"</p></body></html>");

  // Retrieve the IAccessible interface for the web page.
  base::win::ScopedComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.get());
  ASSERT_EQ(1u, document_children.size());

  base::win::ScopedComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.get(), document_children[0].AsInput())
          .get(),
      paragraph.Receive()));
  LONG paragraph_role = 0;
  ASSERT_HRESULT_SUCCEEDED(paragraph->role(&paragraph_role));
  ASSERT_EQ(IA2_ROLE_PARAGRAPH, paragraph_role);
  ASSERT_HRESULT_SUCCEEDED(paragraph.QueryInterface(paragraph_text->Receive()));
}

// Static helpers ------------------------------------------------

base::win::ScopedComPtr<IAccessible>
AccessibilityWinBrowserTest::GetAccessibleFromVariant(
    IAccessible* parent,
    VARIANT* var) {
  base::win::ScopedComPtr<IAccessible> ptr;
  switch (V_VT(var)) {
    case VT_DISPATCH: {
      IDispatch* dispatch = V_DISPATCH(var);
      if (dispatch)
        dispatch->QueryInterface(ptr.Receive());
      break;
    }

    case VT_I4: {
      base::win::ScopedComPtr<IDispatch> dispatch;
      HRESULT hr = parent->get_accChild(*var, dispatch.Receive());
      EXPECT_TRUE(SUCCEEDED(hr));
      if (dispatch.get())
        dispatch.QueryInterface(ptr.Receive());
      break;
    }
  }
  return ptr;
}

HRESULT AccessibilityWinBrowserTest::QueryIAccessible2(
    IAccessible* accessible,
    IAccessible2** accessible2) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.Receive());
  return SUCCEEDED(hr) ?
      service_provider->QueryService(IID_IAccessible2, accessible2) : hr;
}

// Recursively search through all of the descendants reachable from an
// IAccessible node and return true if we find one with the given role
// and name.
void AccessibilityWinBrowserTest::FindNodeInAccessibilityTree(
    IAccessible* node,
    int32_t expected_role,
    const std::wstring& expected_name,
    int32_t depth,
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
         base::WideToUTF8(IAccessibleRoleToString(V_I4(role.ptr()))).c_str(),
         base::WideToUTF8(name).c_str());

  if (expected_role == V_I4(role.ptr()) && expected_name == name) {
    *found = true;
    return;
  }

  std::vector<base::win::ScopedVariant> children = GetAllAccessibleChildren(
      node);
  for (size_t i = 0; i < children.size(); ++i) {
    base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromVariant(node, children[i].AsInput()));
    if (child_accessible) {
      FindNodeInAccessibilityTree(
          child_accessible.get(), expected_role, expected_name, depth + 1,
          found);
      if (*found)
        return;
    }
  }
}

// Ensures that the text and the start and end offsets retrieved using
// get_textAtOffset match the expected values.
void AccessibilityWinBrowserTest::CheckTextAtOffset(
    base::win::ScopedComPtr<IAccessibleText>& object,
    LONG offset,
    IA2TextBoundaryType boundary_type,
    LONG expected_start_offset,
    LONG expected_end_offset,
    const std::wstring& expected_text) {
  testing::Message message;
  message << "While checking for \'" << expected_text << "\' at " <<
      expected_start_offset << '-' << expected_end_offset << '.';
  SCOPED_TRACE(message);

  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  HRESULT hr = object->get_textAtOffset(offset, boundary_type, &start_offset,
                                        &end_offset, text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(expected_start_offset, start_offset);
  EXPECT_EQ(expected_end_offset, end_offset);
  EXPECT_EQ(expected_text, std::wstring(text, text.Length()));
}

std::vector<base::win::ScopedVariant>
AccessibilityWinBrowserTest::GetAllAccessibleChildren(
    IAccessible* element) {
  LONG child_count = 0;
  HRESULT hr = element->get_accChildCount(&child_count);
  EXPECT_EQ(S_OK, hr);
  if (child_count <= 0)
      return std::vector<base::win::ScopedVariant>();

  std::unique_ptr<VARIANT[]> children_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(
      element, 0, child_count, children_array.get(), &obtained_count);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(child_count, obtained_count);

  std::vector<base::win::ScopedVariant> children(
      static_cast<size_t>(child_count));
  for (size_t i = 0; i < children.size(); i++) {
    children[i].Reset(children_array[i]);
  }
  return children;
}


// AccessibleChecker ----------------------------------------------------------

class AccessibilityWinBrowserTest::AccessibleChecker {
 public:
  // This constructor can be used if the IA2 role will be the same as the MSAA
  // role.
  AccessibleChecker(const std::wstring& expected_name,
                    int32_t expected_role,
                    const std::wstring& expected_value);
  AccessibleChecker(const std::wstring& expected_name,
                    int32_t expected_role,
                    int32_t expected_ia2_role,
                    const std::wstring& expected_value);
  AccessibleChecker(const std::wstring& expected_name,
                    const std::wstring& expected_role,
                    int32_t expected_ia2_role,
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
  int32_t ia2_role_;

  // Expected accessible value. Checked against IAccessible::get_accValue.
  std::wstring value_;

  // Expected accessible state. Checked against IAccessible::get_accState.
  LONG state_;

  // Expected accessible children. Checked using IAccessible::get_accChildCount
  // and ::AccessibleChildren.
  AccessibleCheckerVector children_;
};

AccessibilityWinBrowserTest::AccessibleChecker::AccessibleChecker(
    const std::wstring& expected_name,
    int32_t expected_role,
    const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_role),
      value_(expected_value),
      state_(-1) {}

AccessibilityWinBrowserTest::AccessibleChecker::AccessibleChecker(
    const std::wstring& expected_name,
    int32_t expected_role,
    int32_t expected_ia2_role,
    const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_ia2_role),
      value_(expected_value),
      state_(-1) {}

AccessibilityWinBrowserTest::AccessibleChecker::AccessibleChecker(
    const std::wstring& expected_name,
    const std::wstring& expected_role,
    int32_t expected_ia2_role,
    const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role.c_str()),
      ia2_role_(expected_ia2_role),
      value_(expected_value),
      state_(-1) {}

void AccessibilityWinBrowserTest::AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessible(
    IAccessible* accessible) {
  SCOPED_TRACE("While checking "
                   + base::UTF16ToUTF8(RoleVariantToString(role_)));
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckIA2Role(accessible);
  CheckAccessibleValue(accessible);
  CheckAccessibleState(accessible);
  CheckAccessibleChildren(accessible);
}

void AccessibilityWinBrowserTest::AccessibleChecker::SetExpectedValue(
    const std::wstring& expected_value) {
  value_ = expected_value;
}

void AccessibilityWinBrowserTest::AccessibleChecker::SetExpectedState(
    LONG expected_state) {
  state_ = expected_state;
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleName(
    IAccessible* accessible) {
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

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleRole(
    IAccessible* accessible) {
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(0, role_.Compare(role))
      << "Expected role: " << RoleVariantToString(role_)
      << "\nGot role: " << RoleVariantToString(role);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckIA2Role(
    IAccessible* accessible) {
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

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleValue(
    IAccessible* accessible) {
  // Don't check the value if if's a DOCUMENT role, because the value
  // is supposed to be the url (and we don't keep track of that in the
  // test expectations).
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);
  if (role.type() == VT_I4 && V_I4(role.ptr()) == ROLE_SYSTEM_DOCUMENT)
    return;

  // Get the value.
  base::win::ScopedBstr value;
  hr = accessible->get_accValue(childid_self, value.Receive());
  EXPECT_EQ(S_OK, hr);

  // Test that the correct string was returned.
  EXPECT_EQ(value_, std::wstring(value, value.Length()));
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleState(
    IAccessible* accessible) {
  if (state_ < 0)
    return;

  base::win::ScopedVariant state;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accState(childid_self, state.Receive());
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(VT_I4, state.type());
  LONG obj_state = V_I4(state.ptr());
  // Avoid flakiness. The "offscreen" state depends on whether the browser
  // window is frontmost or not, and "hottracked" depends on whether the
  // mouse cursor happens to be over the element.
  obj_state &= ~(STATE_SYSTEM_OFFSCREEN | STATE_SYSTEM_HOTTRACKED);
  EXPECT_EQ(state_, obj_state)
    << "Expected state: " << IAccessibleStateToString(state_)
    << "\nGot state: " << IAccessibleStateToString(obj_state);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleChildren(
    IAccessible* parent) {
  std::vector<base::win::ScopedVariant> obtained_children =
      GetAllAccessibleChildren(parent);
  size_t child_count = obtained_children.size();
  ASSERT_EQ(child_count, children_.size());

  AccessibleCheckerVector::iterator child_checker;
  std::vector<base::win::ScopedVariant>::iterator child;
  for (child_checker = children_.begin(),
       child = obtained_children.begin();
       child_checker != children_.end()
       && child != obtained_children.end();
       ++child_checker, ++child) {
    base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromVariant(parent, child->AsInput()));
    ASSERT_TRUE(child_accessible.get());
    (*child_checker)->CheckAccessible(child_accessible.get());
  }
}

base::string16
AccessibilityWinBrowserTest::AccessibleChecker::RoleVariantToString(
    const base::win::ScopedVariant& role) {
  if (role.type() == VT_I4)
    return IAccessibleRoleToString(V_I4(role.ptr()));
  if (role.type() == VT_BSTR)
    return base::string16(V_BSTR(role.ptr()), SysStringLen(V_BSTR(role.ptr())));
  return base::string16();
}

}  // namespace


// Tests ----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestBusyAccessibilityTree) {
 if (GetBaseAccessibilityMode() != AccessibilityModeOff)
    return;

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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
                                          AccessibilityModeComplete,
                                          ui::AX_EVENT_FOCUS));
  ExecuteScript(L"document.body.children[0].focus()");
  waiter->WaitForNotification();

  // Check that the accessibility tree of the browser has been updated.
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the active descendant of the radio group
  waiter.reset(new AccessibilityNotificationWaiter(
      shell()->web_contents(), AccessibilityModeComplete,
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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
                                          AccessibilityModeComplete,
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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
                                          AccessibilityModeComplete,
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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
                                          AccessibilityModeComplete,
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
          shell()->web_contents(), AccessibilityModeComplete,
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
  std::unique_ptr<AccessibilityNotificationWaiter> waiter(
      new AccessibilityNotificationWaiter(shell()->web_contents(),
                                          AccessibilityModeComplete,
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
// flaky: http://crbug.com/402190
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_ContainsRendererAccessibilityTree) {
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
  FindNodeInAccessibilityTree(
      browser_accessible.get(), ROLE_SYSTEM_DOCUMENT, L"MyDocument", 0, &found);
  ASSERT_EQ(found, true);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SupportsISimpleDOM) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox'></body>");

  // Get the IAccessible object for the document.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));

  // Get the ISimpleDOM object for the document.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = static_cast<IAccessible*>(document_accessible.get())
                   ->QueryInterface(service_provider.Receive());
  ASSERT_EQ(S_OK, hr);
  const GUID refguid = {0x0c539790,
                        0x12e4,
                        0x11cf,
                        {0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8}};
  base::win::ScopedComPtr<ISimpleDOMNode> document_isimpledomnode;
  hr = static_cast<IServiceProvider*>(service_provider.get())
           ->QueryService(
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
  EXPECT_EQ(1u, num_children);
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
  EXPECT_EQ(1u, num_children);
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
  EXPECT_EQ(0u, num_children);
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

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsWithInvalidArguments) {
  base::win::ScopedComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  LONG invalid_offset = -3;
  LONG x = -1, y = -1;
  LONG width = -1, height = -1;

  HRESULT hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_PARENT_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);

  invalid_offset = n_characters + 1;
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_PARENT_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestCharacterExtents) {
  base::win::ScopedComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  const LONG newline_offset = 46;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);

  LONG x, y, width, height;
  LONG previous_x, previous_y;

  for (int coordinate = IA2_COORDTYPE_SCREEN_RELATIVE;
       coordinate <= IA2_COORDTYPE_PARENT_RELATIVE; ++coordinate) {
    auto coordinate_type = static_cast<IA2CoordinateType>(coordinate);
    EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
        0, coordinate_type, &x, &y, &width, &height));
    EXPECT_LE(0, x) << "at offset 0";
    EXPECT_LE(0, y) << "at offset 0";
    EXPECT_LT(0, width) << "at offset 0";
    EXPECT_LT(0, height) << "at offset 0";

    for (LONG offset = 1; offset < newline_offset; ++offset) {
      previous_x = x;
      previous_y = y;

      EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(0, width) << "at offset " << offset;
      EXPECT_LT(0, height) << "at offset " << offset;
    }

    EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
        newline_offset + 1, coordinate_type, &x, &y, &width, &height));
    EXPECT_LE(0, x) << "at offset " << newline_offset + 1;
    EXPECT_GT(previous_x, x) << "at offset " << newline_offset + 1;
    EXPECT_LT(previous_y, y) << "at offset " << newline_offset + 1;
    EXPECT_LT(0, width) << "at offset " << newline_offset + 1;
    EXPECT_LT(0, height) << "at offset " << newline_offset + 1;

    for (LONG offset = newline_offset + 2; offset < n_characters; ++offset) {
      previous_x = x;
      previous_y = y;

      EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(0, width) << "at offset " << offset;
      EXPECT_LT(0, height) << "at offset " << offset;
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestSetCaretOffset) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  LONG caret_offset = 0;
  HRESULT hr = input_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(CONTENTS_LENGTH - 1, caret_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      AccessibilityModeComplete,
      ui::AX_EVENT_TEXT_SELECTION_CHANGED);
  caret_offset = 0;
  hr = input_text->setCaretOffset(caret_offset);
  EXPECT_EQ(S_OK, hr);
  waiter.WaitForNotification();

  hr = input_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, caret_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineSetCaretOffset) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

    LONG caret_offset = 0;
  HRESULT hr = textarea_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(CONTENTS_LENGTH - 1, caret_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      AccessibilityModeComplete,
      ui::AX_EVENT_TEXT_SELECTION_CHANGED);
  caret_offset = 0;
  hr = textarea_text->setCaretOffset(caret_offset);
  EXPECT_EQ(S_OK, hr);
  waiter.WaitForNotification();

  hr = textarea_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, caret_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithInvalidArguments) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);
  HRESULT hr = input_text->get_textAtOffset(
      0, IA2_TEXT_BOUNDARY_CHAR, NULL, NULL, NULL);
  EXPECT_EQ(E_INVALIDARG, hr);

  // Test invalid offset.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  LONG invalid_offset = -5;
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  invalid_offset = CONTENTS_LENGTH + 1;
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  invalid_offset = CONTENTS_LENGTH;
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_ALL,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));

  // The same behavior should be observed when the special offset
  // IA2_TEXT_OFFSET_LENGTH is used.
  hr = input_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = input_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_ALL,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithInvalidArguments) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);
  HRESULT hr = textarea_text->get_textAtOffset(
      0, IA2_TEXT_BOUNDARY_CHAR, NULL, NULL, NULL);
  EXPECT_EQ(E_INVALIDARG, hr);

  // Test invalid offset.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  LONG invalid_offset = -5;
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  invalid_offset = CONTENTS_LENGTH + 1;
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  invalid_offset = CONTENTS_LENGTH;
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_ALL,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));

  // The same behavior should be observed when the special offset
  // IA2_TEXT_OFFSET_LENGTH is used.
  hr = textarea_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_CHAR,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_WORD,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
  hr = textarea_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_ALL,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, static_cast<BSTR>(text));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryCharacter) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);
  for (LONG offset = 0; offset < CONTENTS_LENGTH; ++offset) {
    std::wstring expected_text(1, INPUT_CONTENTS[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_CHAR,
        expected_start_offset, expected_end_offset, expected_text);
  }

  for (LONG offset = CONTENTS_LENGTH - 1; offset >= 0; --offset) {
    std::wstring expected_text(1, INPUT_CONTENTS[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_CHAR,
        expected_start_offset, expected_end_offset, expected_text);
  }

  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET,
      IA2_TEXT_BOUNDARY_CHAR, CONTENTS_LENGTH - 1, CONTENTS_LENGTH, L".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
    TestMultiLineTextAtOffsetWithBoundaryCharacter) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);
  for (LONG offset = 0; offset < CONTENTS_LENGTH; ++offset) {
    std::wstring expected_text(1, TEXTAREA_CONTENTS[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(textarea_text, offset, IA2_TEXT_BOUNDARY_CHAR,
        expected_start_offset, expected_end_offset, expected_text);
  }

  for (LONG offset = CONTENTS_LENGTH - 1; offset >= 0; --offset) {
    std::wstring expected_text(1, TEXTAREA_CONTENTS[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(textarea_text, offset, IA2_TEXT_BOUNDARY_CHAR,
        expected_start_offset, expected_end_offset, expected_text);
  }

  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
      IA2_TEXT_BOUNDARY_CHAR, CONTENTS_LENGTH - 1, CONTENTS_LENGTH, L".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryWord) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  // Trailing punctuation should be included as part of the previous word.
  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");
  CheckTextAtOffset(input_text, 2, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");

  // If the offset is at the punctuation, it should return
  // the previous word.
  CheckTextAtOffset(input_text, 3, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");

  // Numbers with a decimal point ("." for U.S), should be treated as one word.
  // Also, trailing punctuation that occurs after empty space should be part of
  // the word. ("5.0 (" and not "5.0 ".)
  CheckTextAtOffset(input_text, 4, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(input_text, 5, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(input_text, 6, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(input_text, 7, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");

  // Leading punctuation should not be included with the word after it.
  CheckTextAtOffset(input_text, 8, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(input_text, 11, IA2_TEXT_BOUNDARY_WORD,
      9, 12, L"ST ");

  // Numbers separated from letters with trailing punctuation should
  // be split into two words. Same for abreviations like "i.e.".
  CheckTextAtOffset(input_text, 12, IA2_TEXT_BOUNDARY_WORD,
      12, 14, L"6.");
  CheckTextAtOffset(input_text, 15, IA2_TEXT_BOUNDARY_WORD,
      14, 17, L"x; ");

  // Words with numbers should be treated like ordinary words.
  CheckTextAtOffset(input_text, 17, IA2_TEXT_BOUNDARY_WORD,
      17, 24, L"WWW33) ");
  CheckTextAtOffset(input_text, 23, IA2_TEXT_BOUNDARY_WORD,
      17, 24, L"WWW33) ");

  // Multiple trailing empty spaces should be part of the word preceding it.
  CheckTextAtOffset(input_text, 28, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit  \"");
  CheckTextAtOffset(input_text, 31, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit  \"");
  CheckTextAtOffset(input_text, 32, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit  \"");

  // Leading punctuation such as quotation marks should not be part of the word.
  CheckTextAtOffset(input_text, 33, IA2_TEXT_BOUNDARY_WORD,
      33, 40, L"KHTML, ");
  CheckTextAtOffset(input_text, 38, IA2_TEXT_BOUNDARY_WORD,
      33, 40, L"KHTML, ");

  // Trailing final punctuation should be part of the last word.
  CheckTextAtOffset(input_text, 41, IA2_TEXT_BOUNDARY_WORD,
      40, CONTENTS_LENGTH, L"like\".");
  CheckTextAtOffset(input_text, 45, IA2_TEXT_BOUNDARY_WORD,
      40, CONTENTS_LENGTH, L"like\".");

  // Test special offsets.
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET,
      IA2_TEXT_BOUNDARY_WORD, 40, CONTENTS_LENGTH, L"like\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
    TestMultiLineTextAtOffsetWithBoundaryWord) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  // Trailing punctuation should be included as part of the previous word.
  CheckTextAtOffset(textarea_text, 0, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");
  CheckTextAtOffset(textarea_text, 2, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");

  // If the offset is at the punctuation, it should return
  // the previous word.
  CheckTextAtOffset(textarea_text, 3, IA2_TEXT_BOUNDARY_WORD,
      0, 4, L"Moz/");

  // Numbers with a decimal point ("." for U.S), should be treated as one word.
  // Also, trailing punctuation that occurs after empty space should be part of
  // the word. ("5.0 (" and not "5.0 ".)
  CheckTextAtOffset(textarea_text, 4, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(textarea_text, 5, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(textarea_text, 6, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(textarea_text, 7, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");

  // Leading punctuation should not be included with the word after it.
  CheckTextAtOffset(textarea_text, 8, IA2_TEXT_BOUNDARY_WORD,
      4, 9, L"5.0 (");
  CheckTextAtOffset(textarea_text, 11, IA2_TEXT_BOUNDARY_WORD,
      9, 12, L"ST ");

  // Numbers separated from letters with trailing punctuation should
  // be split into two words. Same for abreviations like "i.e.".
  CheckTextAtOffset(textarea_text, 12, IA2_TEXT_BOUNDARY_WORD,
      12, 14, L"6.");
  CheckTextAtOffset(textarea_text, 15, IA2_TEXT_BOUNDARY_WORD,
      14, 17, L"x; ");

  // Words with numbers should be treated like ordinary words.
  CheckTextAtOffset(textarea_text, 17, IA2_TEXT_BOUNDARY_WORD,
      17, 24, L"WWW33)\n");
  CheckTextAtOffset(textarea_text, 23, IA2_TEXT_BOUNDARY_WORD,
      17, 24, L"WWW33)\n");

  // Multiple trailing empty spaces should be part of the word preceding it.
  CheckTextAtOffset(textarea_text, 28, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit \n\"");
  CheckTextAtOffset(textarea_text, 31, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit \n\"");
  CheckTextAtOffset(textarea_text, 32, IA2_TEXT_BOUNDARY_WORD,
      24, 33, L"WebKit \n\"");

  // Leading punctuation such as quotation marks should not be part of the word.
  CheckTextAtOffset(textarea_text, 33, IA2_TEXT_BOUNDARY_WORD,
      33, 40, L"KHTML, ");
  CheckTextAtOffset(textarea_text, 38, IA2_TEXT_BOUNDARY_WORD,
      33, 40, L"KHTML, ");

  // Trailing final punctuation should be part of the last word.
  CheckTextAtOffset(textarea_text, 41, IA2_TEXT_BOUNDARY_WORD,
      40, CONTENTS_LENGTH, L"like\".");
  CheckTextAtOffset(textarea_text, 45, IA2_TEXT_BOUNDARY_WORD,
      40, CONTENTS_LENGTH, L"like\".");

  // Test special offsets.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
      IA2_TEXT_BOUNDARY_WORD, 40, CONTENTS_LENGTH, L"like\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestStaticTextAtOffsetWithBoundaryWord) {
  base::win::ScopedComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);
  base::string16 embedded_character(
      1, BrowserAccessibilityWin::kEmbeddedCharacter);
  std::vector<std::wstring> words;
  words.push_back(L"Game ");
  words.push_back(L"theory ");
  words.push_back(L"is \"");
  words.push_back(L"the ");
  words.push_back(L"study ");
  words.push_back(L"of " + embedded_character + L' ');
  words.push_back(L"of ");
  words.push_back(L"conflict ");
  words.push_back(L"and\n");
  words.push_back(L"cooperation ");
  words.push_back(L"between ");
  words.push_back(L"intelligent ");
  words.push_back(L"rational ");
  words.push_back(L"decision-");
  words.push_back(L"makers.\"");

  // Try to retrieve one word after another.
  LONG word_start_offset = 0;
  for (auto& word : words) {
    LONG word_end_offset = word_start_offset + word.size();
    CheckTextAtOffset(paragraph_text, word_start_offset, IA2_TEXT_BOUNDARY_WORD,
                      word_start_offset, word_end_offset, word);
    word_start_offset = word_end_offset;
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundarySentence) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  // Sentence navigation is not currently implemented.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  HRESULT hr = input_text->get_textAtOffset(
      5, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundarySentence) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  // Sentence navigation is not currently implemented.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  HRESULT hr = textarea_text->get_textAtOffset(
      25, IA2_TEXT_BOUNDARY_SENTENCE,
      &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_FALSE, hr);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryLine) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  // Single line text fields should return the whole text.
  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_LINE,
      0, CONTENTS_LENGTH, base::SysUTF8ToWide(INPUT_CONTENTS));

  // Test special offsets.
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_LINE,
      0, CONTENTS_LENGTH, base::SysUTF8ToWide(INPUT_CONTENTS));
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET, IA2_TEXT_BOUNDARY_LINE,
      0, CONTENTS_LENGTH, base::SysUTF8ToWide(INPUT_CONTENTS));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
    TestMultiLineTextAtOffsetWithBoundaryLine) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  CheckTextAtOffset(textarea_text, 0, IA2_TEXT_BOUNDARY_LINE,
      0, 24, L"Moz/5.0 (ST 6.x; WWW33)\n");

  // If the offset is at the newline, return the line preceding it.
  CheckTextAtOffset(textarea_text, 31, IA2_TEXT_BOUNDARY_LINE,
      24, 32, L"WebKit \n");

  // Last line does not have a trailing newline.
  CheckTextAtOffset(textarea_text, 32, IA2_TEXT_BOUNDARY_LINE,
      32, CONTENTS_LENGTH, L"\"KHTML, like\".");

  // An offset one past the last character should return the last line.
  CheckTextAtOffset(textarea_text, CONTENTS_LENGTH, IA2_TEXT_BOUNDARY_LINE,
      32, CONTENTS_LENGTH, L"\"KHTML, like\".");

  // Test special offsets.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_LENGTH,
      IA2_TEXT_BOUNDARY_LINE, 32, CONTENTS_LENGTH, L"\"KHTML, like\".");
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
      IA2_TEXT_BOUNDARY_LINE, 32, CONTENTS_LENGTH, L"\"KHTML, like\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestParagraphTextAtOffsetWithBoundaryLine) {
  base::win::ScopedComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  // There should be two lines in this paragraph.
  const LONG newline_offset = 46;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);
  ASSERT_LT(newline_offset, n_characters);

  for (LONG i = 0; i <= newline_offset; ++i) {
    CheckTextAtOffset(
        paragraph_text, i, IA2_TEXT_BOUNDARY_LINE, 0, newline_offset + 1,
        L"Game theory is \"the study of \xFFFC of conflict and\n");
  }

  // For line boundaries, IA2 Spec allows for the offset to be equal to the
  // text's length.
  for (LONG i = newline_offset + 1; i <= n_characters; ++i) {
    CheckTextAtOffset(
        paragraph_text, i, IA2_TEXT_BOUNDARY_LINE, newline_offset + 1,
        n_characters,
        L"cooperation between intelligent rational decision-makers.\"");
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryAll) {
  base::win::ScopedComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_ALL,
      0, CONTENTS_LENGTH, base::SysUTF8ToWide(INPUT_CONTENTS));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryAll) {
  base::win::ScopedComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  CheckTextAtOffset(textarea_text, CONTENTS_LENGTH - 1, IA2_TEXT_BOUNDARY_ALL,
      0, CONTENTS_LENGTH, base::SysUTF8ToWide(TEXTAREA_CONTENTS));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestIAccessibleAction) {
  LoadInitialAccessibilityTreeFromHtml(
      "<!DOCTYPE html><html><body>"
      "<img src=\"\" alt=\"image\" "
      "onclick=\"document.querySelector('img').alt = 'image2';\">"
      "</body></html>");

  // Retrieve the IAccessible interface for the web page.
  base::win::ScopedComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.get());
  ASSERT_EQ(1u, document_children.size());

  base::win::ScopedComPtr<IAccessible2> div;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.get(), document_children[0].AsInput())
          .get(),
      div.Receive()));
  std::vector<base::win::ScopedVariant> div_children =
      GetAllAccessibleChildren(div.get());
  ASSERT_EQ(1u, div_children.size());

  base::win::ScopedComPtr<IAccessible2> image;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(div.get(), div_children[0].AsInput()).get(),
      image.Receive()));
  LONG image_role = 0;
  ASSERT_HRESULT_SUCCEEDED(image->role(&image_role));
  ASSERT_EQ(ROLE_SYSTEM_GRAPHIC, image_role);

  base::win::ScopedComPtr<IAccessibleAction> image_action;
  ASSERT_HRESULT_SUCCEEDED(image.QueryInterface(image_action.Receive()));

  LONG n_actions = 0;
  EXPECT_HRESULT_SUCCEEDED(image_action->nActions(&n_actions));
  EXPECT_EQ(1, n_actions);

  base::win::ScopedBstr action_name;
  EXPECT_HRESULT_SUCCEEDED(image_action->get_name(0, action_name.Receive()));
  EXPECT_EQ(L"click", std::wstring(action_name, action_name.Length()));
  action_name.Release();
  EXPECT_HRESULT_FAILED(image_action->get_name(1, action_name.Receive()));
  EXPECT_EQ(nullptr, static_cast<BSTR>(action_name));

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr image_name;
  EXPECT_HRESULT_SUCCEEDED(
      image->get_accName(childid_self, image_name.Receive()));
  EXPECT_EQ(L"image", std::wstring(image_name, image_name.Length()));
  image_name.Release();
  // Cllicking the image will change its name.
  EXPECT_HRESULT_SUCCEEDED(image_action->doAction(0));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         AccessibilityModeComplete,
                                         ui::AX_EVENT_TEXT_CHANGED);
  waiter.WaitForNotification();
  EXPECT_HRESULT_SUCCEEDED(
      image->get_accName(childid_self, image_name.Receive()));
  EXPECT_EQ(L"image2", std::wstring(image_name, image_name.Length()));
  EXPECT_HRESULT_FAILED(image_action->doAction(1));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, HasHWNDAfterNavigation) {
  // This test simulates a scenario where RenderWidgetHostViewAura::SetSize
  // is not called again after its window is added to the root window.
  // Ensure that we still get a legacy HWND for accessibility.

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  WebContentsView* web_contents_view = web_contents->GetView();
  WebContentsViewAura* web_contents_view_aura =
      static_cast<WebContentsViewAura*>(web_contents_view);

  // Set a flag that will cause WebContentsViewAura to initialize a
  // RenderWidgetHostViewAura with a null parent view.
  web_contents_view_aura->set_init_rwhv_with_null_parent_for_testing(true);

  // Navigate to a new page and wait for the accessibility tree to load.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      AccessibilityModeComplete,
      ui::AX_EVENT_LOAD_COMPLETE);
  NavigateToURL(shell(), embedded_test_server()->GetURL(
      "/accessibility/html/article.html"));
  waiter.WaitForNotification();

  // At this point the root of the accessibility tree shouldn't have an HWND
  // because we never gave a parent window to the RWHVA.
  BrowserAccessibilityManagerWin* manager =
      static_cast<BrowserAccessibilityManagerWin*>(
          web_contents->GetRootBrowserAccessibilityManager());
  ASSERT_EQ(nullptr, manager->GetParentHWND());

  // Now add the RWHVA's window to the root window and ensure that we have
  // an HWND for accessibility now.
  web_contents_view->GetNativeView()->AddChild(
      web_contents->GetRenderWidgetHostView()->GetNativeView());
  ASSERT_NE(nullptr, manager->GetParentHWND());
}

}  // namespace content
