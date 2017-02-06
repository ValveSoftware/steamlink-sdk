// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_VIEW_IMPL_H_
#define SERVICES_NAVIGATION_VIEW_IMPL_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection_ref.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class WebView;
class Widget;
}

namespace navigation {

class ViewImpl : public mojom::View,
                 public content::WebContentsDelegate,
                 public content::NotificationObserver,
                 public mus::WindowTreeClientDelegate,
                 public views::WidgetDelegate {
 public:
  ViewImpl(shell::Connector* connector,
           const std::string& client_user_id,
           mojom::ViewClientPtr client,
           mojom::ViewRequest request,
           std::unique_ptr<shell::ShellConnectionRef> ref);
  ~ViewImpl() override;

 private:
  // mojom::View:
  void NavigateTo(const GURL& url) override;
  void GoBack() override;
  void GoForward() override;
  void NavigateToOffset(int offset) override;
  void Reload(bool skip_cache) override;
  void Stop() override;
  void GetWindowTreeClient(
      mus::mojom::WindowTreeClientRequest request) override;
  void ShowInterstitial(const mojo::String& html) override;
  void HideInterstitial() override;
  void SetResizerSize(const gfx::Size& size) override;

  // content::WebContentsDelegate:
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  gfx::Rect GetRootWindowResizerRect() const override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // mus::WindowTreeClientDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) override;
  void OnEventObserved(const ui::Event& event, mus::Window* target) override;

  // views::WidgetDelegate:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  shell::Connector* connector_;
  mojo::StrongBinding<mojom::View> binding_;
  mojom::ViewClientPtr client_;
  std::unique_ptr<shell::ShellConnectionRef> ref_;

  views::WebView* web_view_;

  std::unique_ptr<content::WebContents> web_contents_;

  content::NotificationRegistrar registrar_;

  std::unique_ptr<views::Widget> widget_;

  gfx::Size resizer_size_;

  DISALLOW_COPY_AND_ASSIGN(ViewImpl);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_VIEW_IMPL_H_
