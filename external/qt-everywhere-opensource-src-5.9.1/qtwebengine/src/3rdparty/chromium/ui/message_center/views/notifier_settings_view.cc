// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notifier_settings_view.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

// Additional views-specific parameters.

// The width of the settings pane in pixels.
const int kWidth = 360;

// The width of the learn more icon in pixels.
const int kLearnMoreSize = 12;

// The width of the click target that contains the learn more button in pixels.
const int kLearnMoreTargetWidth = 28;

// The height of the click target that contains the learn more button in pixels.
const int kLearnMoreTargetHeight = 40;

// The minimum height of the settings pane in pixels.
const int kMinimumHeight = 480;

// The horizontal margin of the title area of the settings pane in addition to
// the standard margin from kHorizontalMargin.
const int kTitleMargin = 20;

// The innate vertical blank space in the label for the title of the settings
// pane.
const int kInnateTitleBottomMargin = 1;
const int kInnateTitleTopMargin = 7;

// The innate top blank space in the label for the description of the settings
// pane.
const int kInnateDescriptionTopMargin = 2;

// Checkboxes have some built-in right padding blank space.
const int kInnateCheckboxRightPadding = 2;

// Spec defines the checkbox size; the innate padding throws this measurement
// off so we need to compute a slightly different area for the checkbox to
// inhabit.
const int kComputedCheckboxSize =
    settings::kCheckboxSizeWithPadding - kInnateCheckboxRightPadding;

// The spec doesn't include the bottom blank area of the title bar or the innate
// blank area in the description label, so we'll use this as the space between
// the title and description.
const int kComputedTitleBottomMargin = settings::kDescriptionToSwitcherSpace -
                                       kInnateTitleBottomMargin -
                                       kInnateDescriptionTopMargin;

// The blank space above the title needs to be adjusted by the amount of blank
// space included in the title label.
const int kComputedTitleTopMargin =
    settings::kTopMargin - kInnateTitleTopMargin;

// The switcher has a lot of blank space built in so we should include that when
// spacing the title area vertically.
const int kComputedTitleElementSpacing =
    settings::kDescriptionToSwitcherSpace - 6;

// A function to create a focus border.
std::unique_ptr<views::Painter> CreateFocusPainter() {
  return views::Painter::CreateSolidFocusPainter(kFocusBorderColor,
                                                 gfx::Insets(1, 2, 3, 2));
}

// EntryView ------------------------------------------------------------------

// The view to guarantee the 48px height and place the contents at the
// middle. It also guarantee the left margin.
class EntryView : public views::View {
 public:
  explicit EntryView(views::View* contents);
  ~EntryView() override;

  // views::View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(EntryView);
};

EntryView::EntryView(views::View* contents)
    : focus_painter_(CreateFocusPainter()) {
  AddChildView(contents);
}

EntryView::~EntryView() {}

void EntryView::Layout() {
  DCHECK_EQ(1, child_count());
  views::View* content = child_at(0);
  int content_width = width();
  int content_height = content->GetHeightForWidth(content_width);
  int y = std::max((height() - content_height) / 2, 0);
  content->SetBounds(0, y, content_width, content_height);
}

gfx::Size EntryView::GetPreferredSize() const {
  DCHECK_EQ(1, child_count());
  gfx::Size size = child_at(0)->GetPreferredSize();
  size.SetToMax(gfx::Size(kWidth, settings::kEntryHeight));
  return size;
}

void EntryView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  DCHECK_EQ(1, child_count());
  child_at(0)->GetAccessibleNodeData(node_data);
}

void EntryView::OnFocus() {
  views::View::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
  // We render differently when focused.
  SchedulePaint();
}

bool EntryView::OnKeyPressed(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyPressed(event);
}

bool EntryView::OnKeyReleased(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyReleased(event);
}

void EntryView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void EntryView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

// NotifierGroupComboboxModel --------------------------------------------------

class NotifierGroupComboboxModel : public ui::ComboboxModel {
 public:
  explicit NotifierGroupComboboxModel(
      NotifierSettingsProvider* notifier_settings_provider);
  ~NotifierGroupComboboxModel() override;

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

 private:
  NotifierSettingsProvider* notifier_settings_provider_;

  DISALLOW_COPY_AND_ASSIGN(NotifierGroupComboboxModel);
};

NotifierGroupComboboxModel::NotifierGroupComboboxModel(
    NotifierSettingsProvider* notifier_settings_provider)
    : notifier_settings_provider_(notifier_settings_provider) {}

NotifierGroupComboboxModel::~NotifierGroupComboboxModel() {}

int NotifierGroupComboboxModel::GetItemCount() const {
  return notifier_settings_provider_->GetNotifierGroupCount();
}

base::string16 NotifierGroupComboboxModel::GetItemAt(int index) {
  const NotifierGroup& group =
      notifier_settings_provider_->GetNotifierGroupAt(index);
  return group.login_info.empty() ? group.name : group.login_info;
}

}  // namespace

// NotifierSettingsView::NotifierButton ---------------------------------------

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
NotifierSettingsView::NotifierButton::NotifierButton(
    NotifierSettingsProvider* provider,
    Notifier* notifier,
    views::ButtonListener* listener)
    : views::CustomButton(listener),
      provider_(provider),
      notifier_(notifier),
      icon_view_(new views::ImageView()),
      name_view_(new views::Label(notifier_->name)),
      checkbox_(new views::Checkbox(base::string16())),
      learn_more_(nullptr) {
  DCHECK(provider);
  DCHECK(notifier);

  // Since there may never be an icon (but that could change at a later time),
  // we own the icon view here.
  icon_view_->set_owned_by_client();

  checkbox_->SetChecked(notifier_->enabled);
  checkbox_->set_listener(this);
  checkbox_->SetFocusBehavior(FocusBehavior::NEVER);
  checkbox_->SetAccessibleName(notifier_->name);

  if (ShouldHaveLearnMoreButton()) {
    // Create a more-info button that will be right-aligned.
    learn_more_ = new views::ImageButton(this);
    learn_more_->SetFocusPainter(CreateFocusPainter());
    learn_more_->SetFocusForPlatform();

    ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    learn_more_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS));
    learn_more_->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS_HOVER));
    learn_more_->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS_PRESSED));
    learn_more_->SetState(views::Button::STATE_NORMAL);
    int learn_more_border_width = (kLearnMoreTargetWidth - kLearnMoreSize) / 2;
    int learn_more_border_height =
        (kLearnMoreTargetHeight - kLearnMoreSize) / 2;
    // The image itself is quite small, this large invisible border creates a
    // much bigger click target.
    learn_more_->SetBorder(views::CreateEmptyBorder(
        learn_more_border_height, learn_more_border_width,
        learn_more_border_height, learn_more_border_width));
    learn_more_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  }

  UpdateIconImage(notifier_->icon);
}

NotifierSettingsView::NotifierButton::~NotifierButton() {
}

void NotifierSettingsView::NotifierButton::UpdateIconImage(
    const gfx::Image& icon) {
  bool has_icon_view = false;

  notifier_->icon = icon;
  if (!icon.IsEmpty()) {
    icon_view_->SetImage(icon.ToImageSkia());
    icon_view_->SetImageSize(
        gfx::Size(settings::kEntryIconSize, settings::kEntryIconSize));
    has_icon_view = true;
  }
  GridChanged(ShouldHaveLearnMoreButton(), has_icon_view);
}

void NotifierSettingsView::NotifierButton::SetChecked(bool checked) {
  checkbox_->SetChecked(checked);
  notifier_->enabled = checked;
}

bool NotifierSettingsView::NotifierButton::checked() const {
  return checkbox_->checked();
}

bool NotifierSettingsView::NotifierButton::has_learn_more() const {
  return learn_more_ != nullptr;
}

const Notifier& NotifierSettingsView::NotifierButton::notifier() const {
  return *notifier_.get();
}

void NotifierSettingsView::NotifierButton::SendLearnMorePressedForTest() {
  if (learn_more_ == nullptr)
    return;
  gfx::Point point(110, 120);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  ButtonPressed(learn_more_, pressed);
}

void NotifierSettingsView::NotifierButton::ButtonPressed(
    views::Button* button,
    const ui::Event& event) {
  if (button == checkbox_) {
    // The checkbox state has already changed at this point, but we'll update
    // the state on NotifierSettingsView::ButtonPressed() too, so here change
    // back to the previous state.
    checkbox_->SetChecked(!checkbox_->checked());
    CustomButton::NotifyClick(event);
  } else if (button == learn_more_) {
    DCHECK(provider_);
    provider_->OnNotifierAdvancedSettingsRequested(notifier_->notifier_id,
                                                   nullptr);
  }
}

void NotifierSettingsView::NotifierButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  static_cast<views::View*>(checkbox_)->GetAccessibleNodeData(node_data);
}

bool NotifierSettingsView::NotifierButton::ShouldHaveLearnMoreButton() const {
  if (!provider_)
    return false;

  return provider_->NotifierHasAdvancedSettings(notifier_->notifier_id);
}

void NotifierSettingsView::NotifierButton::GridChanged(bool has_learn_more,
                                                       bool has_icon_view) {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* cs = layout->AddColumnSet(0);
  // Add a column for the checkbox.
  cs->AddPaddingColumn(0, kInnateCheckboxRightPadding);
  cs->AddColumn(GridLayout::CENTER,
                GridLayout::CENTER,
                0,
                GridLayout::FIXED,
                kComputedCheckboxSize,
                0);
  cs->AddPaddingColumn(0, settings::kInternalHorizontalSpacing);

  if (has_icon_view) {
    // Add a column for the icon.
    cs->AddColumn(GridLayout::CENTER,
                  GridLayout::CENTER,
                  0,
                  GridLayout::FIXED,
                  settings::kEntryIconSize,
                  0);
    cs->AddPaddingColumn(0, settings::kInternalHorizontalSpacing);
  }

  // Add a column for the name.
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);

  // Add a padding column which contains expandable blank space.
  cs->AddPaddingColumn(1, 0);

  // Add a column for the learn more button if necessary.
  if (has_learn_more) {
    cs->AddPaddingColumn(0, settings::kInternalHorizontalSpacing);
    cs->AddColumn(
        GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(0, 0);
  layout->AddView(checkbox_);
  if (has_icon_view)
    layout->AddView(icon_view_.get());
  layout->AddView(name_view_);
  if (has_learn_more)
    layout->AddView(learn_more_);

  Layout();
}


// NotifierSettingsView -------------------------------------------------------

NotifierSettingsView::NotifierSettingsView(NotifierSettingsProvider* provider)
    : title_arrow_(nullptr),
      title_label_(nullptr),
      notifier_group_combobox_(nullptr),
      scroller_(nullptr),
      provider_(provider) {
  // |provider_| may be null in tests.
  if (provider_)
    provider_->AddObserver(this);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));
  SetPaintToLayer(true);

  title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL),
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  title_label_->SetBorder(
      views::CreateEmptyBorder(kComputedTitleTopMargin, kTitleMargin,
                               kComputedTitleBottomMargin, kTitleMargin));

  AddChildView(title_label_);

  scroller_ = new views::ScrollView();
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  AddChildView(scroller_);

  std::vector<Notifier*> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(notifiers);
}

NotifierSettingsView::~NotifierSettingsView() {
  // |provider_| may be null in tests.
  if (provider_)
    provider_->RemoveObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::UpdateIconImage(const NotifierId& notifier_id,
                                           const gfx::Image& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end();
       ++iter) {
    if ((*iter)->notifier().notifier_id == notifier_id) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::NotifierGroupChanged() {
  std::vector<Notifier*> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(notifiers);
}

void NotifierSettingsView::NotifierEnabledChanged(const NotifierId& notifier_id,
                                                  bool enabled) {}

void NotifierSettingsView::UpdateContentsView(
    const std::vector<Notifier*>& notifiers) {
  buttons_.clear();

  views::View* contents_view = new views::View();
  contents_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, settings::kHorizontalMargin, 0, 0));

  views::View* contents_title_view = new views::View();
  contents_title_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kComputedTitleElementSpacing));

  bool need_account_switcher =
      provider_ && provider_->GetNotifierGroupCount() > 1;
  int top_label_resource_id =
      need_account_switcher ? IDS_MESSAGE_CENTER_SETTINGS_DESCRIPTION_MULTIUSER
                            : IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION;

  views::Label* top_label =
      new views::Label(l10n_util::GetStringUTF16(top_label_resource_id));
  top_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kTitleMargin - settings::kHorizontalMargin)));
  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label->SetMultiLine(true);

  contents_title_view->AddChildView(top_label);

  if (need_account_switcher) {
    const NotifierGroup& active_group = provider_->GetActiveNotifierGroup();
    base::string16 notifier_group_text = active_group.login_info.empty() ?
        active_group.name : active_group.login_info;
    notifier_group_model_.reset(new NotifierGroupComboboxModel(provider_));
    notifier_group_combobox_ = new views::Combobox(notifier_group_model_.get());
    notifier_group_combobox_->set_listener(this);

    // Move the combobox over enough to align with the checkboxes.
    views::View* combobox_spacer = new views::View();
    combobox_spacer->SetLayoutManager(new views::FillLayout());
    int padding = views::LabelButtonAssetBorder::GetDefaultInsetsForStyle(
                      views::LabelButton::STYLE_TEXTBUTTON)
                      .left() -
                  notifier_group_combobox_->border()->GetInsets().left();
    combobox_spacer->SetBorder(views::CreateEmptyBorder(0, padding, 0, 0));
    combobox_spacer->AddChildView(notifier_group_combobox_);

    contents_title_view->AddChildView(combobox_spacer);
  }

  contents_view->AddChildView(contents_title_view);

  size_t notifier_count = notifiers.size();
  for (size_t i = 0; i < notifier_count; ++i) {
    NotifierButton* button = new NotifierButton(provider_, notifiers[i], this);
    EntryView* entry = new EntryView(button);

    // This code emulates separators using borders.  We will create an invisible
    // border on the last notifier, as the spec leaves a space for it.
    std::unique_ptr<views::Border> entry_border;
    if (i == notifier_count - 1) {
      entry_border =
          views::CreateEmptyBorder(0, 0, settings::kEntrySeparatorHeight, 0);
    } else {
      entry_border =
          views::CreateSolidSidedBorder(0, 0, settings::kEntrySeparatorHeight,
                                        0, settings::kEntrySeparatorColor);
    }
    entry->SetBorder(std::move(entry_border));
    entry->SetFocusBehavior(FocusBehavior::ALWAYS);
    contents_view->AddChildView(entry);
    buttons_.insert(button);
  }

  scroller_->SetContents(contents_view);

  contents_view->SetBoundsRect(gfx::Rect(contents_view->GetPreferredSize()));
  InvalidateLayout();
}

void NotifierSettingsView::Layout() {
  int title_height = title_label_->GetHeightForWidth(width());
  title_label_->SetBounds(0, 0, width(), title_height);

  views::View* contents_view = scroller_->contents();
  int content_width = width();
  int content_height = contents_view->GetHeightForWidth(content_width);
  if (title_height + content_height > height()) {
    content_width -= scroller_->GetScrollBarWidth();
    content_height = contents_view->GetHeightForWidth(content_width);
  }
  contents_view->SetBounds(0, 0, content_width, content_height);
  scroller_->SetBounds(0, title_height, width(), height() - title_height);
}

gfx::Size NotifierSettingsView::GetMinimumSize() const {
  gfx::Size size(kWidth, kMinimumHeight);
  int total_height = title_label_->GetPreferredSize().height() +
                     scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumHeight)
    size.Enlarge(scroller_->GetScrollBarWidth(), 0);
  return size;
}

gfx::Size NotifierSettingsView::GetPreferredSize() const {
  gfx::Size preferred_size;
  gfx::Size title_size = title_label_->GetPreferredSize();
  gfx::Size content_size = scroller_->contents()->GetPreferredSize();
  return gfx::Size(std::max(title_size.width(), content_size.width()),
                   title_size.height() + content_size.height());
}

bool NotifierSettingsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    GetWidget()->Close();
    return true;
  }

  return scroller_->OnKeyPressed(event);
}

bool NotifierSettingsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return scroller_->OnMouseWheel(event);
}

void NotifierSettingsView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  if (sender == title_arrow_) {
    MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
    center_view->SetSettingsVisible(!center_view->settings_visible());
    return;
  }

  std::set<NotifierButton*>::iterator iter =
      buttons_.find(static_cast<NotifierButton*>(sender));

  if (iter == buttons_.end())
    return;

  (*iter)->SetChecked(!(*iter)->checked());
  if (provider_)
    provider_->SetNotifierEnabled((*iter)->notifier(), (*iter)->checked());
}

void NotifierSettingsView::OnPerformAction(views::Combobox* combobox) {
  provider_->SwitchToNotifierGroup(combobox->selected_index());
  MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
  center_view->OnSettingsChanged();
}

}  // namespace message_center
