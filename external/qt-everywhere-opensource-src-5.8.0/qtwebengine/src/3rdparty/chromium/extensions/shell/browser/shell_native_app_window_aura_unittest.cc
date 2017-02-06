// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window_aura.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

class ShellNativeAppWindowAuraTest : public ExtensionsTest {
 public:
  ShellNativeAppWindowAuraTest()
      : notification_service_(content::NotificationService::Create()) {
    AppWindowClient::Set(&app_window_client_);
  }

  ~ShellNativeAppWindowAuraTest() override {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<content::NotificationService> notification_service_;
  ShellAppWindowClient app_window_client_;
};

TEST_F(ShellNativeAppWindowAuraTest, Bounds) {
  // The BrowserContext used here must be destroyed before the thread bundle,
  // because of destructors of things spawned from creating a WebContents.
  std::unique_ptr<content::BrowserContext> browser_context(
      new content::TestBrowserContext);
  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "test extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  AppWindow* app_window = new AppWindow(
      browser_context.get(), new ShellAppDelegate, extension.get());
  content::WebContents* web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser_context.get()));
  app_window->SetAppWindowContentsForTesting(
      base::WrapUnique(new TestAppWindowContents(web_contents)));

  AppWindow::BoundsSpecification window_spec;
  window_spec.bounds = gfx::Rect(100, 200, 300, 400);
  AppWindow::CreateParams params;
  params.window_spec = window_spec;

  ShellNativeAppWindowAura window(app_window, params);

  gfx::Rect bounds = window.GetBounds();
  EXPECT_EQ(window_spec.bounds, bounds);

  // Delete the AppWindow.
  app_window->OnNativeClose();
}

}  // namespace extensions
