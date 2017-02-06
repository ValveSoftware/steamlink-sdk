// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/webtest/webtest.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/c/system/main.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace webtest {

class UI : public views::WidgetDelegateView,
           public navigation::mojom::ViewClient {
 public:
  UI(Webtest* webtest,
     navigation::mojom::ViewPtr view,
     navigation::mojom::ViewClientRequest request)
      : webtest_(webtest),
        view_(std::move(view)),
        view_client_binding_(this, std::move(request)) {}
  ~UI() override {
    webtest_->RemoveWindow(GetWidget());
  }

  void NavigateTo(const GURL& url) {
    view_->NavigateTo(url);
  }

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    if (current_title_.empty())
      return base::ASCIIToUTF16("navigation::View client");
    base::string16 format = base::ASCIIToUTF16("%s - navigation::View client");
    base::ReplaceFirstSubstringAfterOffset(
        &format, 0, base::ASCIIToUTF16("%s"), current_title_);
    return format;
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect local_bounds = GetLocalBounds();
    if (content_area_) {
      gfx::Point offset = local_bounds.origin();
      ConvertPointToWidget(this, &offset);
      int width = local_bounds.width();
      int height = local_bounds.height();
      content_area_->SetBounds(
          gfx::Rect(offset.x(), offset.y(), width, height));
    }
  }
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && GetWidget() && !content_area_) {
      mus::Window* window = aura::GetMusWindow(GetWidget()->GetNativeWindow());
      content_area_ = window->window_tree()->NewWindow(nullptr);
      window->AddChild(content_area_);

      mus::mojom::WindowTreeClientPtr client;
      view_->GetWindowTreeClient(GetProxy(&client));
      content_area_->Embed(std::move(client));
    }
  }

  // navigation::mojom::ViewClient:
  void OpenURL(navigation::mojom::OpenURLParamsPtr params) override {}
  void LoadingStateChanged(bool is_loading) override {}
  void NavigationStateChanged(const GURL& url,
                              const mojo::String& title,
                              bool can_go_back,
                              bool can_go_forward) override {
    current_title_ = base::UTF8ToUTF16(title.get());
    GetWidget()->UpdateWindowTitle();
  }
  void LoadProgressChanged(double progress) override {}
  void UpdateHoverURL(const GURL& url) override {}
  void ViewCreated(navigation::mojom::ViewPtr view,
                   navigation::mojom::ViewClientRequest request,
                   bool is_popup,
                   const gfx::Rect& initial_rect,
                   bool user_gesture) override {
    views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
        new UI(webtest_, std::move(view), std::move(request)), nullptr,
        initial_rect);
    window->Show();
    webtest_->AddWindow(window);
  }
  void Close() override {
    GetWidget()->Close();
  }
  void NavigationPending(navigation::mojom::NavigationEntryPtr entry) override {
  }
  void NavigationCommitted(
      navigation::mojom::NavigationCommittedDetailsPtr details,
      int current_index) override {}
  void NavigationEntryChanged(navigation::mojom::NavigationEntryPtr entry,
                              int entry_index) override {}
  void NavigationListPruned(bool from_front, int count) override {}

  Webtest* webtest_;
  mus::Window* content_area_ = nullptr;
  navigation::mojom::ViewPtr view_;
  mojo::Binding<navigation::mojom::ViewClient> view_client_binding_;
  base::string16 current_title_;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

Webtest::Webtest() {}
Webtest::~Webtest() {}

void Webtest::AddWindow(views::Widget* window) {
  windows_.push_back(window);
}

void Webtest::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

void Webtest::Initialize(shell::Connector* connector,
                         const shell::Identity& identity,
                         uint32_t id) {
  connector_ = connector;
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);
}

bool Webtest::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Launchable>(this);
  return true;
}

void Webtest::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse = how == mojom::LaunchMode::REUSE ||
               how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }

  navigation::mojom::ViewFactoryPtr view_factory;
  connector_->ConnectToInterface("exe:navigation", &view_factory);
  navigation::mojom::ViewPtr view;
  navigation::mojom::ViewClientPtr view_client;
  navigation::mojom::ViewClientRequest view_client_request =
      GetProxy(&view_client);
  view_factory->CreateView(std::move(view_client), GetProxy(&view));
  UI* ui = new UI(this, std::move(view), std::move(view_client_request));
  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      ui, nullptr, gfx::Rect(50, 10, 600, 600));
  ui->NavigateTo(GURL("http://www.theverge.com/"));
  window->Show();
  AddWindow(window);
}

void Webtest::Create(shell::Connection* connection,
                     mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace webtest
}  // namespace mash
