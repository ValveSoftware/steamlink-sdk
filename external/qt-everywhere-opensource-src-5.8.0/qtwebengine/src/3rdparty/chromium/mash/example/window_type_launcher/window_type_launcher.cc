// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/example/window_type_launcher/window_type_launcher.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::MenuItemView;
using views::MenuRunner;

namespace {

SkColor g_colors[] = { SK_ColorRED,
                       SK_ColorYELLOW,
                       SK_ColorBLUE,
                       SK_ColorGREEN };
int g_color_index = 0;

// WidgetDelegateView implementation used for new windows.
class WindowDelegateView : public views::WidgetDelegateView {
 public:
  enum Traits {
    RESIZABLE = 1 << 0,
    ALWAYS_ON_TOP = 1 << 1,
    PANEL = 1 << 2,
  };

  explicit WindowDelegateView(uint32_t traits) : traits_(traits) {}
  ~WindowDelegateView() override {}

  // Creates and shows a window with the specified traits.
  static void Create(uint32_t traits) {
    // Widget destroys itself when closed or mus::Window destroyed.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        (traits & PANEL) != 0 ? views::Widget::InitParams::TYPE_PANEL
                              : views::Widget::InitParams::TYPE_WINDOW);
    if ((traits & PANEL) != 0) {
      params
          .mus_properties[mus::mojom::WindowManager::kInitialBounds_Property] =
          mojo::TypeConverter<std::vector<uint8_t>, gfx::Rect>::Convert(
              gfx::Rect(100, 100, 300, 300));
    }
    params.keep_on_top = (traits & ALWAYS_ON_TOP) != 0;
    // WidgetDelegateView deletes itself when Widget is destroyed.
    params.delegate = new WindowDelegateView(traits);
    widget->Init(params);
    widget->Show();
  }

  // WidgetDelegateView:
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  bool CanResize() const override { return (traits_ & RESIZABLE) != 0; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Window");
  }

 private:
  const uint32_t traits_;

  DISALLOW_COPY_AND_ASSIGN(WindowDelegateView);
};

class ModalWindow : public views::WidgetDelegateView,
                    public views::ButtonListener {
 public:
  explicit ModalWindow(ui::ModalType modal_type)
      : modal_type_(modal_type),
        color_(g_colors[g_color_index]),
        open_button_(new views::LabelButton(this,
                                            base::ASCIIToUTF16("Moar!"))) {
    ++g_color_index %= arraysize(g_colors);
    open_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(open_button_);
  }
  ~ModalWindow() override {}

  static void OpenModalWindow(aura::Window* parent, ui::ModalType modal_type) {
    views::Widget* widget = views::Widget::CreateWindowWithParent(
        new ModalWindow(modal_type), parent);
    widget->GetNativeView()->SetName("ModalWindow");
    widget->Show();
  }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size GetPreferredSize() const override { return gfx::Size(200, 200); }
  void Layout() override {
    gfx::Size open_ps = open_button_->GetPreferredSize();
    gfx::Rect local_bounds = GetLocalBounds();
    open_button_->SetBounds(
        5, local_bounds.bottom() - open_ps.height() - 5,
        open_ps.width(), open_ps.height());
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Modal Window");
  }
  ui::ModalType GetModalType() const override { return modal_type_; }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == open_button_);
    OpenModalWindow(modal_type_ == ui::MODAL_TYPE_SYSTEM
                        ? nullptr
                        : GetWidget()->GetNativeView(),
                    modal_type_);
  }

 private:
  ui::ModalType modal_type_;
  SkColor color_;
  views::LabelButton* open_button_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class NonModalTransient : public views::WidgetDelegateView {
 public:
  NonModalTransient()
      : color_(g_colors[g_color_index]) {
    ++g_color_index %= arraysize(g_colors);
  }
  ~NonModalTransient() override {}

  static void OpenNonModalTransient(aura::Window* parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
    widget->GetNativeView()->SetName("NonModalTransient");
    widget->Show();
  }

  static void ToggleNonModalTransient(aura::Window* parent) {
    if (!non_modal_transient_) {
      non_modal_transient_ =
          views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
      non_modal_transient_->GetNativeView()->SetName("NonModalTransient");
    }
    if (non_modal_transient_->IsVisible())
      non_modal_transient_->Hide();
    else
      non_modal_transient_->Show();
  }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size GetPreferredSize() const override { return gfx::Size(250, 250); }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Non-Modal Transient");
  }
  void DeleteDelegate() override {
    if (GetWidget() == non_modal_transient_)
      non_modal_transient_ = NULL;

    delete this;
  }

 private:
  SkColor color_;

  static views::Widget* non_modal_transient_;

  DISALLOW_COPY_AND_ASSIGN(NonModalTransient);
};

// static
views::Widget* NonModalTransient::non_modal_transient_ = NULL;

void AddViewToLayout(views::GridLayout* layout, views::View* view) {
  layout->StartRow(0, 0);
  layout->AddView(view);
  layout->AddPaddingRow(0, 5);
}

// The contents view/delegate of a window that shows some buttons that create
// various window types.
class WindowTypeLauncherView : public views::WidgetDelegateView,
                               public views::ButtonListener,
                               public views::MenuDelegate,
                               public views::ContextMenuController {
 public:
  explicit WindowTypeLauncherView(WindowTypeLauncher* window_type_launcher,
                                  shell::Connector* connector)
      : window_type_launcher_(window_type_launcher),
        connector_(connector),
        create_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Create Window"))),
        always_on_top_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Create Always On Top Window"))),
        panel_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Create Panel"))),
        create_nonresizable_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Create Non-Resizable Window"))),
        bubble_button_(
            new views::LabelButton(this,
                                   base::ASCIIToUTF16("Create Pointy Bubble"))),
        lock_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Lock Screen"))),
        logout_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Log Out"))),
        switch_user_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Switch User"))),
        widgets_button_(
            new views::LabelButton(this,
                                   base::ASCIIToUTF16("Show Example Widgets"))),
        system_modal_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Open System Modal Window"))),
        window_modal_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Open Window Modal Window"))),
        child_modal_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Open Child Modal Window"))),
        transient_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Open Non-Modal Transient Window"))),
        examples_button_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Open Views Examples Window"))),
        show_hide_window_button_(
            new views::LabelButton(this,
                                   base::ASCIIToUTF16("Show/Hide a Window"))),
        show_web_notification_(new views::LabelButton(
            this,
            base::ASCIIToUTF16("Show a web/app notification"))),
        jank_button_(new views::LabelButton(
            this, base::ASCIIToUTF16("Jank for (s):"))),
        jank_duration_field_(new views::Textfield) {
    create_button_->SetStyle(views::Button::STYLE_BUTTON);
    always_on_top_button_->SetStyle(views::Button::STYLE_BUTTON);
    panel_button_->SetStyle(views::Button::STYLE_BUTTON);
    create_nonresizable_button_->SetStyle(views::Button::STYLE_BUTTON);
    bubble_button_->SetStyle(views::Button::STYLE_BUTTON);
    lock_button_->SetStyle(views::Button::STYLE_BUTTON);
    logout_button_->SetStyle(views::Button::STYLE_BUTTON);
    switch_user_button_->SetStyle(views::Button::STYLE_BUTTON);
    widgets_button_->SetStyle(views::Button::STYLE_BUTTON);
    system_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
    window_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
    child_modal_button_->SetStyle(views::Button::STYLE_BUTTON);
    transient_button_->SetStyle(views::Button::STYLE_BUTTON);
    examples_button_->SetStyle(views::Button::STYLE_BUTTON);
    show_hide_window_button_->SetStyle(views::Button::STYLE_BUTTON);
    show_web_notification_->SetStyle(views::Button::STYLE_BUTTON);
    jank_button_->SetStyle(views::Button::STYLE_BUTTON);
    jank_duration_field_->SetText(base::ASCIIToUTF16("5"));

    views::GridLayout* layout = new views::GridLayout(this);
    layout->SetInsets(5, 5, 5, 5);
    SetLayoutManager(layout);
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::LEADING,
                          views::GridLayout::CENTER,
                          0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    views::ColumnSet* label_field_set = layout->AddColumnSet(1);
    label_field_set->AddColumn(views::GridLayout::LEADING,
                               views::GridLayout::CENTER,
                               0,
                               views::GridLayout::USE_PREF,
                               0,
                               0);
    label_field_set->AddPaddingColumn(0, 5);
    label_field_set->AddColumn(views::GridLayout::FILL,
                               views::GridLayout::CENTER,
                               0,
                               views::GridLayout::FIXED,
                               75,
                               0);

    AddViewToLayout(layout, create_button_);
    AddViewToLayout(layout, always_on_top_button_);
    AddViewToLayout(layout, panel_button_);
    AddViewToLayout(layout, create_nonresizable_button_);
    AddViewToLayout(layout, bubble_button_);
    AddViewToLayout(layout, lock_button_);
    AddViewToLayout(layout, logout_button_);
    AddViewToLayout(layout, switch_user_button_);
    AddViewToLayout(layout, widgets_button_);
    AddViewToLayout(layout, system_modal_button_);
    AddViewToLayout(layout, window_modal_button_);
    AddViewToLayout(layout, child_modal_button_);
    AddViewToLayout(layout, transient_button_);
    AddViewToLayout(layout, examples_button_);
    AddViewToLayout(layout, show_hide_window_button_);
    AddViewToLayout(layout, show_web_notification_);

    layout->StartRow(0, 1);
    layout->AddView(jank_button_);
    layout->AddView(jank_duration_field_);
    layout->AddPaddingRow(0, 5);

    set_context_menu_controller(this);
  }
  ~WindowTypeLauncherView() override {
    window_type_launcher_->RemoveWindow(GetWidget());
  }

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  enum MenuCommands {
    COMMAND_NEW_WINDOW = 1,
    COMMAND_TOGGLE_FULLSCREEN = 3,
  };

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
  }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Overridden so we get OnMouseReleased and can show the context menu.
    return true;
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this;  }
  bool CanResize() const override { return true;  }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Examples: Window Builder");
  }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == create_button_) {
      WindowDelegateView::Create(WindowDelegateView::RESIZABLE);
    } else if (sender == always_on_top_button_) {
      WindowDelegateView::Create(WindowDelegateView::RESIZABLE |
                                 WindowDelegateView::ALWAYS_ON_TOP);
    } else if (sender == panel_button_) {
      WindowDelegateView::Create(WindowDelegateView::PANEL);
    } else if (sender == create_nonresizable_button_) {
      WindowDelegateView::Create(0u);
    } else if (sender == bubble_button_) {
      NOTIMPLEMENTED();
    } else if (sender == lock_button_) {
      mash::session::mojom::SessionPtr session;
      connector_->ConnectToInterface("mojo:mash_session", &session);
      session->LockScreen();
    } else if (sender == logout_button_) {
      mash::session::mojom::SessionPtr session;
      connector_->ConnectToInterface("mojo:mash_session", &session);
      session->Logout();
    } else if (sender == switch_user_button_) {
      mash::session::mojom::SessionPtr session;
      connector_->ConnectToInterface("mojo:mash_session", &session);
      session->SwitchUser();
    } else if (sender == widgets_button_) {
      NOTIMPLEMENTED();
    }
    else if (sender == system_modal_button_) {
      ModalWindow::OpenModalWindow(nullptr, ui::MODAL_TYPE_SYSTEM);
    } else if (sender == window_modal_button_) {
      ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                   ui::MODAL_TYPE_WINDOW);
    } else if (sender == child_modal_button_) {
    } else if (sender == transient_button_) {
      NonModalTransient::OpenNonModalTransient(GetWidget()->GetNativeView());
    } else if (sender == show_hide_window_button_) {
      NonModalTransient::ToggleNonModalTransient(GetWidget()->GetNativeView());
    } else if (sender == jank_button_) {
      int64_t val;
      base::StringToInt64(jank_duration_field_->text(), &val);
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(val));
    }
  }

  // Overridden from views::MenuDelegate:
  void ExecuteCommand(int id, int event_flags) override {
    switch (id) {
      case COMMAND_NEW_WINDOW:
        NOTIMPLEMENTED();
        break;
      case COMMAND_TOGGLE_FULLSCREEN:
        GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
        break;
      default:
        break;
    }
  }

  // Override from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override {
    MenuItemView* root = new MenuItemView(this);
    root->AppendMenuItem(COMMAND_NEW_WINDOW,
                         base::ASCIIToUTF16("New Window"),
                         MenuItemView::NORMAL);
    root->AppendMenuItem(COMMAND_TOGGLE_FULLSCREEN,
                         base::ASCIIToUTF16("Toggle FullScreen"),
                         MenuItemView::NORMAL);
    // MenuRunner takes ownership of root.
    menu_runner_.reset(new MenuRunner(
        root, MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
    if (menu_runner_->RunMenuAt(GetWidget(),
                                NULL,
                                gfx::Rect(point, gfx::Size()),
                                views::MENU_ANCHOR_TOPLEFT,
                                source_type) == MenuRunner::MENU_DELETED) {
      return;
    }
  }

  WindowTypeLauncher* window_type_launcher_;
  shell::Connector* connector_;
  views::LabelButton* create_button_;
  views::LabelButton* always_on_top_button_;
  views::LabelButton* panel_button_;
  views::LabelButton* create_nonresizable_button_;
  views::LabelButton* bubble_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* logout_button_;
  views::LabelButton* switch_user_button_;
  views::LabelButton* widgets_button_;
  views::LabelButton* system_modal_button_;
  views::LabelButton* window_modal_button_;
  views::LabelButton* child_modal_button_;
  views::LabelButton* transient_button_;
  views::LabelButton* examples_button_;
  views::LabelButton* show_hide_window_button_;
  views::LabelButton* show_web_notification_;
  views::LabelButton* jank_button_;
  views::Textfield* jank_duration_field_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncherView);
};

}  // namespace

WindowTypeLauncher::WindowTypeLauncher() {}
WindowTypeLauncher::~WindowTypeLauncher() {}

void WindowTypeLauncher::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

void WindowTypeLauncher::Initialize(shell::Connector* connector,
                                    const shell::Identity& identity,
                                    uint32_t id) {
  connector_ = connector;
  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));

  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);
}

bool WindowTypeLauncher::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mash::mojom::Launchable>(this);
  return true;
}

void WindowTypeLauncher::Launch(uint32_t what, mash::mojom::LaunchMode how) {
  bool reuse = how == mash::mojom::LaunchMode::REUSE ||
               how == mash::mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }
  views::Widget* window = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new WindowTypeLauncherView(this, connector_);
  window->Init(params);
  window->Show();
  windows_.push_back(window);
}

void WindowTypeLauncher::Create(shell::Connection* connection,
                                mash::mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}
