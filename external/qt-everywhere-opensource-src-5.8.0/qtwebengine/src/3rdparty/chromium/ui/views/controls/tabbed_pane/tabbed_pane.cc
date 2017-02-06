// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include "base/logging.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(markusheintz|msw): Use NativeTheme colors.
const SkColor kTabTitleColor_Inactive = SkColorSetRGB(0x64, 0x64, 0x64);
const SkColor kTabTitleColor_Active = SK_ColorBLACK;
const SkColor kTabTitleColor_Hovered = SK_ColorBLACK;
const SkColor kTabBorderColor = SkColorSetRGB(0xC8, 0xC8, 0xC8);
const SkScalar kTabBorderThickness = 1.0f;

const gfx::Font::Weight kHoverWeight = gfx::Font::Weight::NORMAL;
const gfx::Font::Weight kActiveWeight = gfx::Font::Weight::BOLD;
const gfx::Font::Weight kInactiveWeight = gfx::Font::Weight::NORMAL;

}  // namespace

namespace views {

// static
const char TabbedPane::kViewClassName[] = "TabbedPane";

// The tab view shown in the tab strip.
class Tab : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents);
  ~Tab() override;

  View* contents() const { return contents_; }

  bool selected() const { return contents_->visible(); }
  void SetSelected(bool selected);

  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

 private:
  enum TabState {
    TAB_INACTIVE,
    TAB_ACTIVE,
    TAB_HOVERED,
  };

  void SetState(TabState tab_state);

  TabbedPane* tabbed_pane_;
  Label* title_;
  gfx::Size preferred_title_size_;
  TabState tab_state_;
  // The content view associated with this tab.
  View* contents_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

// The tab strip shown above the tab contents.
class TabStrip : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  explicit TabStrip(TabbedPane* tabbed_pane);
  ~TabStrip() override;

  // Overridden from View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  TabbedPane* tabbed_pane_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

// static
const char Tab::kViewClassName[] = "Tab";

Tab::Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents)
    : tabbed_pane_(tabbed_pane),
      title_(new Label(
          title,
          ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
              ui::kLabelFontSizeDelta,
              gfx::Font::NORMAL,
              kActiveWeight))),
      tab_state_(TAB_ACTIVE),
      contents_(contents) {
  // Calculate this now while the font list is guaranteed to be bold.
  preferred_title_size_ = title_->GetPreferredSize();

  SetState(TAB_INACTIVE);
  AddChildView(title_);
}

Tab::~Tab() {}

void Tab::SetSelected(bool selected) {
  contents_->SetVisible(selected);
  SetState(selected ? TAB_ACTIVE : TAB_INACTIVE);
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() &&
      GetLocalBounds().Contains(event.location()))
    tabbed_pane_->SelectTab(this);
  return true;
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_HOVERED);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      // Fallthrough.
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tabbed_pane_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
      break;
    default:
      break;
  }
  event->SetHandled();
}

gfx::Size Tab::GetPreferredSize() const {
  gfx::Size size(preferred_title_size_);
  size.Enlarge(21, 9);
  const int kTabMinWidth = 54;
  if (size.width() < kTabMinWidth)
    size.set_width(kTabMinWidth);
  return size;
}

void Tab::Layout() {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(0, 1, 0, 0);
  bounds.ClampToCenteredSize(preferred_title_size_);
  title_->SetBoundsRect(bounds);
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

void Tab::SetState(TabState tab_state) {
  if (tab_state == tab_state_)
    return;
  tab_state_ = tab_state;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (tab_state) {
    case TAB_INACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Inactive);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kInactiveWeight));
      break;
    case TAB_ACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Active);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kActiveWeight));
      break;
    case TAB_HOVERED:
      title_->SetEnabledColor(kTabTitleColor_Hovered);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kHoverWeight));
      break;
  }
  SchedulePaint();
}

// static
const char TabStrip::kViewClassName[] = "TabStrip";

TabStrip::TabStrip(TabbedPane* tabbed_pane) : tabbed_pane_(tabbed_pane) {}

TabStrip::~TabStrip() {}

gfx::Size TabStrip::GetPreferredSize() const {
  gfx::Size size;
  for (int i = 0; i < child_count(); ++i) {
    const gfx::Size child_size = child_at(i)->GetPreferredSize();
    size.SetSize(size.width() + child_size.width(),
                 std::max(size.height(), child_size.height()));
  }
  return size;
}

void TabStrip::Layout() {
  const int kTabOffset = 9;
  int x = kTabOffset;  // Layout tabs with an offset to the tabstrip border.
  for (int i = 0; i < child_count(); ++i) {
    gfx::Size ps = child_at(i)->GetPreferredSize();
    child_at(i)->SetBounds(x, 0, ps.width(), ps.height());
    x = child_at(i)->bounds().right();
  }
}

const char* TabStrip::GetClassName() const {
  return kViewClassName;
}

void TabStrip::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);

  // Draw the TabStrip border.
  SkPaint paint;
  paint.setColor(kTabBorderColor);
  paint.setStrokeWidth(kTabBorderThickness);
  SkScalar line_y = SkIntToScalar(height()) - (kTabBorderThickness / 2);
  SkScalar line_end = SkIntToScalar(width());
  int selected_tab_index = tabbed_pane_->selected_tab_index();
  if (selected_tab_index >= 0) {
    Tab* selected_tab = tabbed_pane_->GetTabAt(selected_tab_index);
    SkPath path;
    SkScalar tab_height =
        SkIntToScalar(selected_tab->height()) - kTabBorderThickness;
    SkScalar tab_width =
        SkIntToScalar(selected_tab->width()) - kTabBorderThickness;
    SkScalar tab_start = SkIntToScalar(selected_tab->GetMirroredX());
    path.moveTo(0, line_y);
    path.rLineTo(tab_start, 0);
    path.rLineTo(0, -tab_height);
    path.rLineTo(tab_width, 0);
    path.rLineTo(0, tab_height);
    path.lineTo(line_end, line_y);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kTabBorderColor);
    paint.setStrokeWidth(kTabBorderThickness);
    canvas->DrawPath(path, paint);
  } else {
    canvas->sk_canvas()->drawLine(0, line_y, line_end, line_y, paint);
  }
}

TabbedPane::TabbedPane()
  : listener_(NULL),
    tab_strip_(new TabStrip(this)),
    contents_(new View()),
    selected_tab_index_(-1) {
#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  AddChildView(tab_strip_);
  AddChildView(contents_);
}

TabbedPane::~TabbedPane() {}

int TabbedPane::GetTabCount() {
  DCHECK_EQ(tab_strip_->child_count(), contents_->child_count());
  return contents_->child_count();
}

View* TabbedPane::GetSelectedTab() {
  return selected_tab_index() < 0 ?
      NULL : GetTabAt(selected_tab_index())->contents();
}

void TabbedPane::AddTab(const base::string16& title, View* contents) {
  AddTabAtIndex(tab_strip_->child_count(), title, contents);
}

void TabbedPane::AddTabAtIndex(int index,
                               const base::string16& title,
                               View* contents) {
  DCHECK(index >= 0 && index <= GetTabCount());
  contents->SetVisible(false);

  tab_strip_->AddChildViewAt(new Tab(this, title, contents), index);
  contents_->AddChildViewAt(contents, index);
  if (selected_tab_index() < 0)
    SelectTabAt(index);

  PreferredSizeChanged();
}

void TabbedPane::SelectTabAt(int index) {
  DCHECK(index >= 0 && index < GetTabCount());
  if (index == selected_tab_index())
    return;

  if (selected_tab_index() >= 0)
    GetTabAt(selected_tab_index())->SetSelected(false);

  selected_tab_index_ = index;
  Tab* tab = GetTabAt(index);
  tab->SetSelected(true);
  tab_strip_->SchedulePaint();

  FocusManager* focus_manager = tab->contents()->GetFocusManager();
  if (focus_manager) {
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && contents_->Contains(focused_view) &&
        !tab->contents()->Contains(focused_view))
      focus_manager->SetFocusedView(tab->contents());
  }

  if (listener())
    listener()->TabSelectedAt(index);
}

void TabbedPane::SelectTab(Tab* tab) {
  const int index = tab_strip_->GetIndexOf(tab);
  if (index >= 0)
    SelectTabAt(index);
}

gfx::Size TabbedPane::GetPreferredSize() const {
  gfx::Size size;
  for (int i = 0; i < contents_->child_count(); ++i)
    size.SetToMax(contents_->child_at(i)->GetPreferredSize());
  size.Enlarge(0, tab_strip_->GetPreferredSize().height());
  return size;
}

Tab* TabbedPane::GetTabAt(int index) {
  return static_cast<Tab*>(tab_strip_->child_at(index));
}

void TabbedPane::Layout() {
  const gfx::Size size = tab_strip_->GetPreferredSize();
  tab_strip_->SetBounds(0, 0, width(), size.height());
  contents_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                       std::max(0, height() - size.height()));
  for (int i = 0; i < contents_->child_count(); ++i)
    contents_->child_at(i)->SetSize(contents_->size());
}

void TabbedPane::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    // Support navigating tabs by Ctrl+Tab and Ctrl+Shift+Tab.
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN));
  }
}

bool TabbedPane::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Handle Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  DCHECK(accelerator.key_code() == ui::VKEY_TAB && accelerator.IsCtrlDown());
  const int tab_count = GetTabCount();
  if (tab_count <= 1)
    return false;
  const int increment = accelerator.IsShiftDown() ? -1 : 1;
  int next_tab_index = (selected_tab_index() + increment) % tab_count;
  // Wrap around.
  if (next_tab_index < 0)
    next_tab_index += tab_count;
  SelectTabAt(next_tab_index);
  return true;
}

const char* TabbedPane::GetClassName() const {
  return kViewClassName;
}

void TabbedPane::OnFocus() {
  View::OnFocus();

  View* selected_tab = GetSelectedTab();
  if (selected_tab) {
    selected_tab->NotifyAccessibilityEvent(
        ui::AX_EVENT_FOCUS, true);
  }
}

void TabbedPane::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TAB_LIST;
}

}  // namespace views
