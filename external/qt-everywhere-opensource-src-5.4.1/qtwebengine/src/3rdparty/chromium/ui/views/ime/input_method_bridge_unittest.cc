// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_input_method_delegate.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase InputMethodBridgeTest;

TEST_F(InputMethodBridgeTest, DestructTest) {
  ui::internal::DummyInputMethodDelegate input_method_delegate;
  ui::InputMethodMinimal input_method(&input_method_delegate);

  GetContext()->SetProperty(aura::client::kRootWindowInputMethodKey,
                            static_cast<ui::InputMethod*>(&input_method));

  Widget* toplevel = new Widget;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  // |child| owns |native_widget|.
  toplevel_params.native_widget = new DesktopNativeWidgetAura(toplevel);
  toplevel->Init(toplevel_params);

  Widget* child = new Widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  child_params.parent = toplevel->GetNativeView();
  // |child| owns |native_widget|.
  child_params.native_widget = new NativeWidgetAura(child);
  child->Init(child_params);

  child->GetInputMethod()->OnFocus();

  toplevel->CloseNow();

  GetContext()->SetProperty(aura::client::kRootWindowInputMethodKey,
                            static_cast<ui::InputMethod*>(NULL));
}

}  // namespace views
