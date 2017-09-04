// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/login/login.h"

#include <map>
#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/user_access_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace login {
namespace {

class Login;

class UI : public views::WidgetDelegateView,
           public views::ButtonListener {
 public:
  static void Show(service_manager::Connector* connector,
                   const service_manager::Identity& identity,
                   Login* login) {
    UI* ui = new UI(login, connector);
    ui->StartWindowManager(identity);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = ui;

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[ui::mojom::WindowManager::kInitialContainerId_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            ash::kShellWindowId_LockScreenContainer);
    ui::Window* window =
        views::WindowManagerConnection::Get()->NewTopLevelWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget, window, ui::mojom::CompositorFrameSinkType::DEFAULT);
    widget->Init(params);
    widget->Show();
  }

 private:
  UI(Login* login, service_manager::Connector* connector)
      : login_(login),
        connector_(connector),
        user_id_1_("00000000-0000-4000-8000-000000000000"),
        user_id_2_("00000000-0000-4000-8000-000000000001"),
        login_button_1_(
            views::MdTextButton::Create(this, base::ASCIIToUTF16("Timothy"))),
        login_button_2_(
            views::MdTextButton::Create(this, base::ASCIIToUTF16("Jimothy"))) {
    set_background(views::Background::CreateSolidBackground(SK_ColorRED));
    AddChildView(login_button_1_);
    AddChildView(login_button_2_);
  }
  ~UI() override {
    // Prevent the window manager from restarting during graceful shutdown.
    mash_wm_connection_->SetConnectionLostClosure(base::Closure());
    base::MessageLoop::current()->QuitWhenIdle();
  }

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Login");
  }
  void DeleteDelegate() override { delete this; }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect button_box = GetLocalBounds();
    button_box.Inset(10, 10);

    gfx::Size ps1 = login_button_1_->GetPreferredSize();
    gfx::Size ps2 = login_button_2_->GetPreferredSize();

    DCHECK(ps1.height() == ps2.height());

    // The 10 is inter-button spacing.
    button_box.set_x((button_box.width() - ps1.width() - ps2.width() - 10) / 2);
    button_box.set_y((button_box.height() - ps1.height()) / 2);

    login_button_1_->SetBounds(button_box.x(), button_box.y(), ps1.width(),
                               ps1.height());
    login_button_2_->SetBounds(login_button_1_->bounds().right() + 10,
                               button_box.y(), ps2.width(), ps2.height());
  }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void StartWindowManager(const service_manager::Identity& identity) {
    mash_wm_connection_ = connector_->Connect("ash");
    mash_wm_connection_->SetConnectionLostClosure(
        base::Bind(&UI::StartWindowManager, base::Unretained(this), identity));
    window_manager_connection_ =
        views::WindowManagerConnection::Create(connector_, identity);
  }

  Login* login_;
  service_manager::Connector* connector_;
  const std::string user_id_1_;
  const std::string user_id_2_;
  views::MdTextButton* login_button_1_;
  views::MdTextButton* login_button_2_;
  std::unique_ptr<service_manager::Connection> mash_wm_connection_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

class Login : public service_manager::Service,
              public service_manager::InterfaceFactory<mojom::Login>,
              public mojom::Login {
 public:
   Login() {}
  ~Login() override {}

  void LoginAs(const std::string& user_id) {
    user_access_manager_->SetActiveUser(user_id);
    mash::init::mojom::InitPtr init;
    context()->connector()->ConnectToInterface("mash_init", &init);
    init->StartService("mash_session", user_id);
  }

 private:
  // service_manager::Service:
  void OnStart() override {
    tracing_.Initialize(context()->connector(), context()->identity().name());

    aura_init_ = base::MakeUnique<views::AuraInit>(
        context()->connector(), context()->identity(),
        "views_mus_resources.pak");

    context()->connector()->ConnectToInterface(ui::mojom::kServiceName,
                                               &user_access_manager_);
    user_access_manager_->SetActiveUser(context()->identity().user_id());
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<mojom::Login>(this);
    return true;
  }

  // service_manager::InterfaceFactory<mojom::Login>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::LoginRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::Login:
  void ShowLoginUI() override {
    UI::Show(context()->connector(), context()->identity(), this);
  }
  void SwitchUser() override {
    UI::Show(context()->connector(), context()->identity(), this);
  }

  void StartWindowManager();

  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  mojo::BindingSet<mojom::Login> bindings_;
  ui::mojom::UserAccessManagerPtr user_access_manager_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

void UI::ButtonPressed(views::Button* sender, const ui::Event& event) {
  // Login...
  if (sender == login_button_1_) {
    login_->LoginAs(user_id_1_);
  } else if (sender == login_button_2_) {
    login_->LoginAs(user_id_2_);
  } else {
    NOTREACHED();
  }
  GetWidget()->Close();
}

}  // namespace

service_manager::Service* CreateLogin() {
  return new Login;
}

}  // namespace login
}  // namespace main
