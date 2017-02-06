// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/screenlock/screenlock.h"

#include "ash/public/interfaces/container.mojom.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
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
  explicit ScreenlockView(shell::Connector* connector)
      : connector_(connector),
        unlock_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Unlock"))) {
    set_background(views::Background::CreateSolidBackground(SK_ColorYELLOW));
    unlock_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(unlock_button_);
  }
  ~ScreenlockView() override {}

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
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
    connector_->ConnectToInterface("mojo:mash_session", &session);
    session->UnlockScreen();
  }

  shell::Connector* connector_;
  views::LabelButton* unlock_button_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockView);
};

}  // namespace

Screenlock::Screenlock() {}
Screenlock::~Screenlock() {}

void Screenlock::Initialize(shell::Connector* connector,
                            const shell::Identity& identity,
                            uint32_t id) {
  tracing_.Initialize(connector, identity.name());

  mash::session::mojom::SessionPtr session;
  connector->ConnectToInterface("mojo:mash_session", &session);
  session->AddScreenlockStateListener(
      bindings_.CreateInterfacePtrAndBind(this));

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new ScreenlockView(connector);

  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ash::mojom::kWindowContainer_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ash::mojom::Container::LOGIN_WINDOWS));
  mus::Window* window =
      views::WindowManagerConnection::Get()->NewWindow(properties);
  params.native_widget = new views::NativeWidgetMus(
      widget, connector, window, mus::mojom::SurfaceType::DEFAULT);
  widget->Init(params);
  widget->Show();
}

void Screenlock::ScreenlockStateChanged(bool screen_locked) {
  if (!screen_locked)
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace screenlock
}  // namespace mash
