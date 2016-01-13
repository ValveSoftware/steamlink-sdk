// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace views {
namespace test {

class NativeViewAcccessibilityWinTest : public ViewsTestBase {
 public:
  NativeViewAcccessibilityWinTest() {}
  virtual ~NativeViewAcccessibilityWinTest() {}

 protected:
  void GetIAccessible2InterfaceForView(View* view, IAccessible2_2** result) {
    ScopedComPtr<IAccessible> view_accessible(
        view->GetNativeViewAccessible());
    ScopedComPtr<IServiceProvider> service_provider;
    ASSERT_EQ(S_OK, view_accessible.QueryInterface(service_provider.Receive()));
    ASSERT_EQ(S_OK,
        service_provider->QueryService(IID_IAccessible2_2, result));
  }
};

TEST_F(NativeViewAcccessibilityWinTest, TextfieldAccessibility) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  widget.SetContentsView(content);

  Textfield* textfield = new Textfield;
  textfield->SetAccessibleName(L"Name");
  textfield->SetText(L"Value");
  content->AddChildView(textfield);

  ScopedComPtr<IAccessible> content_accessible(
      content->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  ScopedComPtr<IDispatch> textfield_dispatch;
  ScopedComPtr<IAccessible> textfield_accessible;
  ScopedVariant child_index(1);
  ASSERT_EQ(S_OK, content_accessible->get_accChild(
      child_index, textfield_dispatch.Receive()));
  ASSERT_EQ(S_OK, textfield_dispatch.QueryInterface(
      textfield_accessible.Receive()));

  ScopedBstr name;
  ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK, textfield_accessible->get_accName(
      childid_self, name.Receive()));
  ASSERT_STREQ(L"Name", name);

  ScopedBstr value;
  ASSERT_EQ(S_OK, textfield_accessible->get_accValue(
      childid_self, value.Receive()));
  ASSERT_STREQ(L"Value", value);

  ScopedBstr new_value(L"New value");
  ASSERT_EQ(S_OK, textfield_accessible->put_accValue(childid_self, new_value));

  ASSERT_STREQ(L"New value", textfield->text().c_str());
}

TEST_F(NativeViewAcccessibilityWinTest, UnattachedWebView) {
  // This is a regression test. Calling get_accChild on the native accessible
  // object for a WebView with no attached WebContents was causing an
  // infinite loop and crash. This test simulates that with an ordinary
  // View that registers itself as a web view with NativeViewAcccessibility.

  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  widget.SetContentsView(content);

  View* web_view = new View;
  content->AddChildView(web_view);
  NativeViewAccessibility::RegisterWebView(web_view);

  ScopedComPtr<IAccessible> web_view_accessible(
      web_view->GetNativeViewAccessible());
  ScopedComPtr<IDispatch> result_dispatch;
  ScopedVariant child_index(-999);
  ASSERT_EQ(E_FAIL, web_view_accessible->get_accChild(
      child_index, result_dispatch.Receive()));

  NativeViewAccessibility::UnregisterWebView(web_view);
}

TEST_F(NativeViewAcccessibilityWinTest, AuraOwnedWidgets) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  ScopedComPtr<IAccessible> root_view_accessible(
      widget.GetRootView()->GetNativeViewAccessible());

  LONG child_count = 0;
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  Widget owned_widget;
  Widget::InitParams owned_init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  owned_init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  owned_init_params.parent = widget.GetNativeView();
  owned_widget.Init(owned_init_params);
  owned_widget.Show();

  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(2L, child_count);
}

TEST_F(NativeViewAcccessibilityWinTest, RetrieveAllAlerts) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  widget.SetContentsView(content);

  View* infobar = new View;
  content->AddChildView(infobar);

  View* infobar2 = new View;
  content->AddChildView(infobar2);

  View* root_view = content->parent();
  ASSERT_EQ(NULL, root_view->parent());

  ScopedComPtr<IAccessible2_2> root_view_accessible;
  GetIAccessible2InterfaceForView(root_view, root_view_accessible.Receive());

  ScopedComPtr<IAccessible2_2> infobar_accessible;
  GetIAccessible2InterfaceForView(infobar, infobar_accessible.Receive());

  ScopedComPtr<IAccessible2_2> infobar2_accessible;
  GetIAccessible2InterfaceForView(infobar2, infobar2_accessible.Receive());

  // Initially, there are no alerts
  ScopedBstr alerts_bstr(L"alerts");
  IUnknown** targets;
  long n_targets;
  ASSERT_EQ(S_FALSE, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(0, n_targets);

  // Fire alert events on the infobars.
  infobar->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  infobar2->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);

  // Now calling get_relationTargetsOfType should retrieve the alerts.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(2, n_targets);
  ASSERT_TRUE(infobar_accessible.IsSameObject(targets[0]));
  ASSERT_TRUE(infobar2_accessible.IsSameObject(targets[1]));
  CoTaskMemFree(targets);

  // If we set max_targets to 1, we should only get the first one.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 1, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(infobar_accessible.IsSameObject(targets[0]));
  CoTaskMemFree(targets);

  // If we delete the first view, we should only get the second one now.
  delete infobar;
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(infobar2_accessible.IsSameObject(targets[0]));
  CoTaskMemFree(targets);
}

}  // namespace test
}  // namespace views
