// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/browser/browser.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "mash/browser/debug_view.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/c/system/main.h"
#include "services/navigation/public/cpp/view.h"
#include "services/navigation/public/cpp/view_delegate.h"
#include "services/navigation/public/cpp/view_observer.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace mash {
namespace browser {

void EnableButton(views::CustomButton* button, bool enabled) {
  button->SetState(enabled ? views::Button::STATE_NORMAL
                           : views::Button::STATE_DISABLED);
}

class Tab;

class TabStripObserver {
 public:
  virtual void OnTabAdded(Tab* added) {}
  virtual void OnTabRemoved(Tab* removed) {}
  virtual void OnTabSelected(Tab* selected) {}
};

class Tab : public views::LabelButton,
            public navigation::ViewObserver,
            public TabStripObserver {
 public:
  class Background : public views::Background {
   public:
    explicit Background(Tab* tab) : tab_(tab) {}
    ~Background() override {}

   private:
    // views::Background:
    void Paint(gfx::Canvas* canvas, views::View* view) const override {
      DCHECK_EQ(view, tab_);
      SkColor bg = tab_->selected() ? SK_ColorGRAY : SK_ColorLTGRAY;
      gfx::Rect lb = view->GetLocalBounds();
      canvas->FillRect(lb, bg);
      if (!tab_->selected()) {
        lb.set_y(lb.bottom() - 1);
        lb.set_height(1);
        canvas->FillRect(lb, SK_ColorGRAY);
      }
    }

    Tab* tab_;

    DISALLOW_COPY_AND_ASSIGN(Background);
  };

  Tab(std::unique_ptr<navigation::View> view,
      views::ButtonListener* listener)
      : views::LabelButton(listener, base::ASCIIToUTF16("Blank")),
        view_(std::move(view)) {
    view_->AddObserver(this);
    set_background(new Background(this));
  }
  ~Tab() override {
    view_->RemoveObserver(this);
  }

  bool selected() const { return selected_; }

  mus::Window* window() { return window_; }
  void SetWindow(mus::Window* window) {
    window_ = window;
    window_->SetVisible(selected_);
    view_->EmbedInWindow(window_);
  }
  navigation::View* view() { return view_.get(); }

 private:
  // views::View:
  gfx::Size GetPreferredSize() const override {
    gfx::Size ps = views::LabelButton::GetPreferredSize();
    ps.set_width(180);
    return ps;
  }

  // navigation::ViewObserver:
  void NavigationStateChanged(navigation::View* view) override {
    if (!view->title().empty())
      SetText(view->title());
  }

  // TabStripObserver:
  void OnTabSelected(Tab* selected) override {
    selected_ = selected == this;
    SetTextColor(views::Button::STATE_NORMAL,
                 selected_ ? SK_ColorWHITE : SK_ColorBLACK);
    SetTextColor(views::Button::STATE_HOVERED,
                 selected_ ? SK_ColorWHITE : SK_ColorBLACK);
    SetTextColor(views::Button::STATE_PRESSED,
                 selected_ ? SK_ColorWHITE : SK_ColorBLACK);
    if (window_)
      window_->SetVisible(selected_);
  }

  mus::Window* window_ = nullptr;
  std::unique_ptr<navigation::View> view_;
  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

class TabStrip : public views::View,
                 public views::ButtonListener {
 public:
  class Delegate {
   public:
    virtual void NewTab() = 0;
  };

  explicit TabStrip(Delegate* delegate)
    : delegate_(delegate),
      tab_container_(new views::View),
      new_tab_button_(
          new views::LabelButton(this, base::ASCIIToUTF16("+"))) {
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 5, 0, 0);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
    layout->SetDefaultFlex(0);
    SetLayoutManager(layout);

    views::BoxLayout* tab_container_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    tab_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    tab_container_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
    tab_container_layout->SetDefaultFlex(0);
    tab_container_->SetLayoutManager(tab_container_layout);
    AddChildView(tab_container_);
    layout->SetFlexForView(tab_container_, 1);
    AddChildView(new_tab_button_);
  }
  ~TabStrip() override {
    for (auto* tab : tabs_)
      RemoveObserver(tab);
  }

  void SetContainerWindow(mus::Window* container) {
    DCHECK(!container_);
    container_ = container;
    for (auto* tab : tabs_) {
      mus::Window* window = container_->window_tree()->NewWindow();
      container_->AddChild(window);
      tab->SetWindow(window);
    }
  }

  void AddTab(std::unique_ptr<navigation::View> view) {
    selected_index_ = static_cast<int>(tabs_.size());
    Tab* tab = new Tab(std::move(view), this);
    // We won't have a WindowTree until we're added to a view hierarchy.
    if (container_) {
      mus::Window* window = container_->window_tree()->NewWindow();
      container_->AddChild(window);
      tab->SetWindow(window);
    }
    AddObserver(tab);
    tabs_.push_back(tab);
    tab_container_->AddChildView(tab);
    FOR_EACH_OBSERVER(TabStripObserver, observers_, OnTabAdded(tab));
    SelectTab(tab);
  }

  void CloseTabForView(navigation::View* view) {
    for (auto it = tabs_.begin(); it != tabs_.end(); ++it) {
      Tab* tab = *it;
      if (tab->view() == view) {
        CloseTab(tab);
        break;
      }
    }
  }

  void CloseTab(Tab* tab) {
    auto it = std::find(tabs_.begin(), tabs_.end(), tab);
    int tab_index = static_cast<int>(it - tabs_.begin());
    if (tab_index < selected_index_)
      --selected_index_;
    DCHECK(it != tabs_.end());
    tabs_.erase(it);
    RemoveObserver(tab);
    tab_container_->RemoveChildView(tab);
    if (tab->selected()) {
      int next_selected_index = selected_index_;
      if (selected_index_ == static_cast<int>(tabs_.size()))
        --next_selected_index;
      if (next_selected_index >= 0)
        SelectTab(tabs_[next_selected_index]);
    }
    Layout();
    FOR_EACH_OBSERVER(TabStripObserver, observers_, OnTabRemoved(tab));
    delete tab;
  }

  bool empty() const { return tabs_.empty(); }

  void SelectTab(Tab* tab) {
    auto it = std::find(tabs_.begin(), tabs_.end(), tab);
    DCHECK(it != tabs_.end());
    selected_index_ = it - tabs_.begin();
    FOR_EACH_OBSERVER(TabStripObserver, observers_, OnTabSelected(tab));
  }
  Tab* selected_tab() {
    return selected_index_ != -1 ? tabs_[selected_index_] : nullptr;
  }

  void AddObserver(TabStripObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(TabStripObserver* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect lb = GetLocalBounds();
    lb.set_y(lb.bottom() - 1);
    lb.set_height(1);
    canvas->FillRect(lb, SK_ColorGRAY);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    auto it = std::find(tabs_.begin(), tabs_.end(), sender);
    if (it != tabs_.end()) {
      if (event.IsControlDown())
        CloseTab(*it);
      else
        SelectTab(*it);
    }
    else if (sender == new_tab_button_ && delegate_)
      delegate_->NewTab();
  }

  Delegate* delegate_;
  views::View* tab_container_;
  views::LabelButton* new_tab_button_;
  std::vector<Tab*> tabs_;
  int selected_index_ = -1;
  base::ObserverList<TabStripObserver> observers_;
  mus::Window* container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

class NavMenuModel : public ui::MenuModel {
 public:
  class Delegate {
   public:
    virtual void NavigateToOffset(int offset) = 0;
  };

  NavMenuModel(const std::vector<navigation::NavigationListItem>& entries,
               Delegate* delegate)
      : navigation_delegate_(delegate), entries_(entries) {}
  ~NavMenuModel() override {}

 private:
  bool HasIcons() const override { return false; }
  int GetItemCount() const override {
    return static_cast<int>(entries_.size());
  }
  ui::MenuModel::ItemType GetTypeAt(int index) const override {
    return ui::MenuModel::TYPE_COMMAND;
  }
  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }
  int GetCommandIdAt(int index) const override {
    return index;
  }
  base::string16 GetLabelAt(int index) const override {
    return entries_[index].title;
  }
  base::string16 GetSublabelAt(int index) const override {
    return base::string16();
  }
  base::string16 GetMinorTextAt(int index) const override {
    return base::string16();
  }
  bool IsItemDynamicAt(int index) const override { return false; }
  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }
  bool IsItemCheckedAt(int index) const override { return false; }
  int GetGroupIdAt(int index) const override { return -1; }
  bool GetIconAt(int index, gfx::Image* icon) override { return false; }
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }
  bool IsEnabledAt(int index) const override { return true; }
  bool IsVisibleAt(int index) const override { return true; }
  ui::MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }
  void HighlightChangedTo(int index) override {}
  void ActivatedAt(int index) override {
    ActivatedAt(index, 0);
  }
  void ActivatedAt(int index, int event_flags) override {
    navigation_delegate_->NavigateToOffset(entries_[index].offset);
  }
  void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) override {
    delegate_ = delegate;
  }
  ui::MenuModelDelegate* GetMenuModelDelegate() const override {
    return delegate_;
  }

  ui::MenuModelDelegate* delegate_ = nullptr;
  Delegate* navigation_delegate_;
  std::vector<navigation::NavigationListItem> entries_;

  DISALLOW_COPY_AND_ASSIGN(NavMenuModel);
};

class NavButton : public views::LabelButton {
 public:
  enum class Type {
    BACK,
    FORWARD
  };

  class ModelProvider {
   public:
    virtual std::unique_ptr<ui::MenuModel> CreateMenuModel(Type type) = 0;
  };

  NavButton(Type type,
            ModelProvider* model_provider,
            views::ButtonListener* listener,
            const base::string16& label)
      : views::LabelButton(listener, label),
        type_(type),
        model_provider_(model_provider),
        show_menu_factory_(this) {}
  ~NavButton() override {}

 private:
  // views::LabelButton overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (IsTriggerableEvent(event) && enabled() &&
        HitTestPoint(event.location())) {
      y_pos_on_lbuttondown_ = event.y();
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&NavButton::ShowMenu, show_menu_factory_.GetWeakPtr(),
                     ui::GetMenuSourceTypeForEvent(event)),
          base::TimeDelta::FromMilliseconds(500));
    }
    return LabelButton::OnMousePressed(event);
  }
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    bool result = LabelButton::OnMouseDragged(event);
    if (show_menu_factory_.HasWeakPtrs()) {
      if (event.y() > y_pos_on_lbuttondown_ + GetHorizontalDragThreshold()) {
        show_menu_factory_.InvalidateWeakPtrs();
        ShowMenu(ui::GetMenuSourceTypeForEvent(event));
      }
    }
    return result;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (IsTriggerableEvent(event))
      show_menu_factory_.InvalidateWeakPtrs();
    LabelButton::OnMouseReleased(event);
  }

  void ShowMenu(ui::MenuSourceType source_type) {
    gfx::Rect local = GetLocalBounds();
    gfx::Point menu_position(local.origin());
    menu_position.Offset(0, local.height() - 1);
    View::ConvertPointToScreen(this, &menu_position);

    model_ = model_provider_->CreateMenuModel(type_);
    menu_model_adapter_.reset(new views::MenuModelAdapter(
        model_.get(),
        base::Bind(&NavButton::OnMenuClosed, base::Unretained(this))));
    menu_model_adapter_->set_triggerable_event_flags(triggerable_event_flags());
    menu_runner_.reset(new views::MenuRunner(
        menu_model_adapter_->CreateMenu(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::ASYNC));
    ignore_result(menu_runner_->RunMenuAt(
        GetWidget(), nullptr, gfx::Rect(menu_position, gfx::Size(0, 0)),
        views::MENU_ANCHOR_TOPLEFT, source_type));
  }

  void OnMenuClosed() {
    SetMouseHandler(nullptr);
    model_.reset();
    menu_runner_.reset();
    menu_model_adapter_.reset();
  }

  Type type_;
  ModelProvider* model_provider_;
  int y_pos_on_lbuttondown_ = 0;
  std::unique_ptr<ui::MenuModel> model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::WeakPtrFactory<NavButton> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavButton);
};

class ProgressBar : public views::View {
 public:
  ProgressBar() {}
  ~ProgressBar() override {}

  void SetProgress(double progress) {
    progress_ = progress;
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect stroke_rect = GetLocalBounds();
    stroke_rect.set_y(stroke_rect.bottom() - 1);
    stroke_rect.set_height(1);
    canvas->FillRect(stroke_rect, SK_ColorGRAY);
    if (progress_ != 0.f) {
      gfx::Rect progress_rect = GetLocalBounds();
      progress_rect.set_width(progress_rect.width() * progress_);
      canvas->FillRect(progress_rect, SK_ColorRED);
    }
  }

  double progress_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(ProgressBar);
};

class Throbber : public views::View {
 public:
  Throbber() : timer_(false, true), weak_factory_(this) {}
  ~Throbber() override {}

  void Start() {
    throbbing_ = true;
    start_time_ = base::TimeTicks::Now();
    SchedulePaint();
    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(30),
        base::Bind(&Throbber::SchedulePaint, weak_factory_.GetWeakPtr()));
  }

  void Stop() {
    throbbing_ = false;
    if (timer_.IsRunning())
      timer_.Stop();
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    if (!throbbing_)
      return;

    gfx::PaintThrobberSpinning(
        canvas, GetLocalBounds(),
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_ThrobberSpinningColor),
        base::TimeTicks::Now() - start_time_);
  }

  bool throbbing_ = false;
  base::TimeTicks start_time_;
  base::Timer timer_;
  base::WeakPtrFactory<Throbber> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Throbber);
};

class UI : public views::WidgetDelegateView,
           public views::ButtonListener,
           public views::TextfieldController,
           public TabStrip::Delegate,
           public TabStripObserver,
           public navigation::ViewDelegate,
           public navigation::ViewObserver,
           public NavButton::ModelProvider,
           public NavMenuModel::Delegate {
 public:
  enum class Type { WINDOW, POPUP };

  UI(Browser* browser, Type type, std::unique_ptr<navigation::View> view)
      : browser_(browser),
        type_(type),
        tab_strip_(new TabStrip(this)),
        back_button_(new NavButton(NavButton::Type::BACK, this, this,
                                   base::ASCIIToUTF16("Back"))),
        forward_button_(new NavButton(NavButton::Type::FORWARD, this, this,
                                      base::ASCIIToUTF16("Forward"))),
        reload_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Reload"))),
        prompt_(new views::Textfield),
        debug_button_(new views::LabelButton(this, base::ASCIIToUTF16("DV"))),
        throbber_(new Throbber),
        progress_bar_(new ProgressBar),
        debug_view_(new DebugView) {
    set_background(views::Background::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    back_button_->set_request_focus_on_press(false);
    forward_button_->set_request_focus_on_press(false);
    reload_button_->set_request_focus_on_press(false);
    AddChildView(tab_strip_);
    AddChildView(back_button_);
    AddChildView(forward_button_);
    AddChildView(reload_button_);
    AddChildView(prompt_);
    AddChildView(debug_button_);
    AddChildView(throbber_);
    AddChildView(progress_bar_);
    AddChildView(debug_view_);

    tab_strip_->AddObserver(this);
    tab_strip_->AddTab(std::move(view));
  }
  ~UI() override {
    browser_->RemoveWindow(GetWidget());
  }

  void NavigateTo(const GURL& url) {
    selected_view()->NavigateToURL(url);
  }

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    if (selected_view()->title().empty())
      return base::ASCIIToUTF16("Browser");
    base::string16 format = base::ASCIIToUTF16("%s - Browser");
    base::ReplaceFirstSubstringAfterOffset(&format, 0, base::ASCIIToUTF16("%s"),
                                           selected_view()->title());
    return format;
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == back_button_) {
      selected_view()->GoBack();
    } else if (sender == forward_button_) {
      selected_view()->GoForward();
    } else if (sender == reload_button_) {
      if (selected_view()->is_loading())
        selected_view()->Stop();
      else
        selected_view()->Reload(false);
    } else if (sender == debug_button_) {
      ToggleDebugView();
    }
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect local_bounds = GetLocalBounds();

    gfx::Size ps = tab_strip_->GetPreferredSize();
    tab_strip_->SetBoundsRect(
        gfx::Rect(0, 5, local_bounds.width(), ps.height()));

    gfx::Rect bounds = local_bounds;
    bounds.set_y(tab_strip_->bounds().bottom());
    bounds.Inset(5, 5);

    ps = back_button_->GetPreferredSize();
    back_button_->SetBoundsRect(
        gfx::Rect(bounds.x(), bounds.y(), ps.width(), ps.height()));
    ps = forward_button_->GetPreferredSize();
    forward_button_->SetBoundsRect(gfx::Rect(back_button_->bounds().right() + 5,
                                             bounds.y(), ps.width(),
                                             ps.height()));
    ps = reload_button_->GetPreferredSize();
    reload_button_->SetBoundsRect(
        gfx::Rect(forward_button_->bounds().right() + 5, bounds.y(), ps.width(),
                  ps.height()));

    ps = prompt_->GetPreferredSize();
    int throbber_size = ps.height();
    gfx::Size debug_ps = debug_button_->GetPreferredSize();
    int prompt_y =
        bounds.y() + (reload_button_->bounds().height() - ps.height()) / 2;
    int width =
        bounds.width() - reload_button_->bounds().right() - throbber_size - 15 -
        debug_ps.width();
    prompt_->SetBoundsRect(gfx::Rect(reload_button_->bounds().right() + 5,
                                     prompt_y, width, ps.height()));

    debug_button_->SetBoundsRect(
        gfx::Rect(prompt_->bounds().right() + 5,
                  prompt_->bounds().y(), debug_ps.width(), debug_ps.height()));

    throbber_->SetBoundsRect(gfx::Rect(debug_button_->bounds().right() + 5,
                                       prompt_->bounds().y(), throbber_size,
                                       throbber_size));

    gfx::Rect progress_bar_rect(local_bounds.x(),
                                back_button_->bounds().bottom() + 5,
                                local_bounds.width(), 2);
    progress_bar_->SetBoundsRect(progress_bar_rect);

    int debug_view_height = 0;
    if (showing_debug_view_)
      debug_view_height = debug_view_->GetPreferredSize().height();
    debug_view_->SetBoundsRect(
        gfx::Rect(local_bounds.x(), local_bounds.height() - debug_view_height,
                  local_bounds.width(), debug_view_height));

    if (content_area_) {
      int x = local_bounds.x();
      int y = type_ == Type::POPUP ? 0 : progress_bar_->bounds().bottom();
      gfx::Point offset(x, y);
      ConvertPointToWidget(this, &offset);
      int width = local_bounds.width();
      int height = local_bounds.height() - y - debug_view_height;
      content_area_->SetBounds(
          gfx::Rect(offset.x(), offset.y(), width, height));
      for (auto* child : content_area_->children())
        child->SetBounds(gfx::Rect(0, 0, width, height));
    }
  }
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && GetWidget() && !content_area_) {
      mus::Window* window = aura::GetMusWindow(GetWidget()->GetNativeWindow());
      content_area_ = window->window_tree()->NewWindow(nullptr);
      content_area_->SetVisible(true);
      window->AddChild(content_area_);
      tab_strip_->SetContainerWindow(content_area_);
    }
  }

  // Overridden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::ET_KEY_PRESSED &&
        key_event.key_code() == ui::VKEY_RETURN)
      selected_view()->NavigateToURL(GURL(prompt_->text()));
    return false;
  }

  // TabStrip::Delegate:
  void NewTab() override {
    tab_strip_->AddTab(browser_->CreateView());
    tab_strip_->selected_tab()->view()->NavigateToURL(GURL("about:blank"));
  }

  // TabStripObserver:
  void OnTabAdded(Tab* added) override {
    added->view()->AddObserver(this);
    added->view()->set_delegate(this);
  }
  void OnTabSelected(Tab* selected) override {
    debug_view_->set_view(selected->view());
    prompt_->SetText(base::UTF8ToUTF16(selected->view()->url().spec()));
    if (GetWidget())
      GetWidget()->UpdateWindowTitle();
  }

  // navigation::ViewDelegate:
  void ViewCreated(navigation::View* source,
                   std::unique_ptr<navigation::View> view,
                   bool is_popup,
                   const gfx::Rect& initial_rect,
                   bool user_gesture) override {
    if (is_popup)
      CreateNewWindow(std::move(view), initial_rect, is_popup);
    else
      tab_strip_->AddTab(std::move(view));
  }
  void Close(navigation::View* source) override {
    tab_strip_->CloseTabForView(source);
    if (tab_strip_->empty())
      GetWidget()->Close();
  }
  void OpenURL(navigation::View* source,
               navigation::mojom::OpenURLParamsPtr params) override {
    switch (params->disposition) {
      case navigation::mojom::WindowOpenDisposition::CURRENT_TAB:
        selected_view()->NavigateToURL(params->url);
        break;
      case navigation::mojom::WindowOpenDisposition::NEW_FOREGROUND_TAB:
        tab_strip_->AddTab(browser_->CreateView());
        tab_strip_->selected_tab()->view()->NavigateToURL(params->url);
        break;
      case navigation::mojom::WindowOpenDisposition::NEW_POPUP:
      case navigation::mojom::WindowOpenDisposition::NEW_WINDOW: {
        std::unique_ptr<navigation::View> view = browser_->CreateView();
        view->NavigateToURL(params->url);
        CreateNewWindow(
            std::move(view), gfx::Rect(),
            params->disposition ==
                navigation::mojom::WindowOpenDisposition::NEW_POPUP);
        break;
      }
      default:
        break;
    }
  }

  // navigation::ViewObserver:
  void LoadingStateChanged(navigation::View* view) override {
    if (view->is_loading()) {
      reload_button_->SetText(base::ASCIIToUTF16("Stop"));
      throbber_->Start();
    } else {
      reload_button_->SetText(base::ASCIIToUTF16("Reload"));
      throbber_->Stop();
      progress_bar_->SetProgress(0.f);
    }
  }
  void LoadProgressChanged(navigation::View* view, double progress) override {
    progress_bar_->SetProgress(progress);
  }
  void NavigationStateChanged(navigation::View* view) override {
    EnableButton(back_button_, view->can_go_back());
    EnableButton(forward_button_, view->can_go_forward());
    prompt_->SetText(base::UTF8ToUTF16(view->url().spec()));
    GetWidget()->UpdateWindowTitle();
  }
  void HoverTargetURLChanged(navigation::View* view, const GURL& url) override {
    if (url.is_valid())
      prompt_->SetText(base::UTF8ToUTF16(url.spec()));
    else
      prompt_->SetText(base::UTF8ToUTF16(selected_view()->url().spec()));
  }

  // NavButton::ModelProvider:
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      NavButton::Type type) override {
    std::vector<navigation::NavigationListItem> entries;
    if (type == NavButton::Type::BACK) {
      selected_view()->GetBackMenuItems(&entries);
    } else {
      selected_view()->GetForwardMenuItems(&entries);
    }
    return base::WrapUnique(new NavMenuModel(entries, this));
  }

  // NavMenuModel::Delegate:
  void NavigateToOffset(int offset) override {
    selected_view()->NavigateToOffset(offset);
  }

  navigation::View* selected_view() {
    return const_cast<navigation::View*>(
        static_cast<const UI*>(this)->selected_view());
  }
  const navigation::View* selected_view() const {
    return tab_strip_->selected_tab()->view();
  }

  void CreateNewWindow(std::unique_ptr<navigation::View> view,
                       const gfx::Rect& initial_bounds,
                       bool is_popup) {
    gfx::Rect bounds = initial_bounds;
    if (bounds.IsEmpty())
      bounds = gfx::Rect(10, 10, 400, 300);
    views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
        new UI(browser_, is_popup ? UI::Type::POPUP : UI::Type::WINDOW,
               std::move(view)),
        nullptr, bounds);
    window->Show();
    browser_->AddWindow(window);
  }

  void ToggleDebugView() {
    showing_debug_view_ = !showing_debug_view_;
    Layout();
  }

  Browser* browser_;

  Type type_;

  TabStrip* tab_strip_;
  views::LabelButton* back_button_;
  views::LabelButton* forward_button_;
  views::LabelButton* reload_button_;
  views::Textfield* prompt_;
  views::LabelButton* debug_button_;
  Throbber* throbber_;
  ProgressBar* progress_bar_;

  mus::Window* content_area_ = nullptr;

  DebugView* debug_view_;
  bool showing_debug_view_ = false;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

Browser::Browser() {}
Browser::~Browser() {}

void Browser::AddWindow(views::Widget* window) {
  windows_.push_back(window);
}

void Browser::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

std::unique_ptr<navigation::View> Browser::CreateView() {
  navigation::mojom::ViewFactoryPtr factory;
  connector_->ConnectToInterface("exe:navigation", &factory);
  return base::WrapUnique(new navigation::View(std::move(factory)));
}

void Browser::Initialize(shell::Connector* connector,
                         const shell::Identity& identity,
                         uint32_t id) {
  connector_ = connector;
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);
}

bool Browser::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Launchable>(this);
  return true;
}

void Browser::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse =
      how == mojom::LaunchMode::REUSE || how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }

  UI* ui = new UI(this, UI::Type::WINDOW, CreateView());
  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      ui, nullptr, gfx::Rect(10, 10, 1024, 600));
  ui->NavigateTo(GURL("http://www.google.com/"));
  window->Show();
  AddWindow(window);
}

void Browser::Create(shell::Connection* connection,
                     mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace browser
}  // namespace mash
