// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/screenlock/screenlock.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace screenlock {
namespace {

class ScreenlockView : public views::WidgetDelegateView,
                       public views::ButtonListener {
 public:
  explicit ScreenlockView(service_manager::Connector* connector)
      : connector_(connector),
        unlock_button_(
            views::MdTextButton::Create(this, base::ASCIIToUTF16("Unlock"))) {
    set_background(views::Background::CreateSolidBackground(SK_ColorYELLOW));
    AddChildView(unlock_button_);
  }
  ~ScreenlockView() override {}

 private:
  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Screenlock");
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(10, 10);

    gfx::Size ps = unlock_button_->GetPreferredSize();
    bounds.set_height(bounds.height() - ps.height() - 10);

    unlock_button_->SetBounds(bounds.width() - ps.width(),
                              bounds.bottom() + 10,
                              ps.width(), ps.height());
  }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, unlock_button_);
    mash::session::mojom::SessionPtr session;
    connector_->ConnectToInterface("mash_session", &session);
    session->UnlockScreen();
  }

  service_manager::Connector* connector_;
  views::MdTextButton* unlock_button_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockView);
};

}  // namespace

Screenlock::Screenlock() {}
Screenlock::~Screenlock() {}

void Screenlock::OnStart() {
  tracing_.Initialize(context()->connector(), context()->identity().name());

  mash::session::mojom::SessionPtr session;
  context()->connector()->ConnectToInterface("mash_session", &session);
  session->AddScreenlockStateListener(
      bindings_.CreateInterfacePtrAndBind(this));

  aura_init_ = base::MakeUnique<views::AuraInit>(
      context()->connector(), context()->identity(), "views_mus_resources.pak");
  window_manager_connection_ = views::WindowManagerConnection::Create(
      context()->connector(), context()->identity());

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new ScreenlockView(context()->connector());

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

bool Screenlock::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  return false;
}

void Screenlock::ScreenlockStateChanged(bool screen_locked) {
  if (!screen_locked)
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace screenlock
}  // namespace mash
