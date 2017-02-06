// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/default_style.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/painter.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// STYLE_ACTION arrow container padding widths.
const int kActionLeftPadding = 12;
const int kActionRightPadding = 11;

// Menu border widths
const int kMenuBorderWidthLeft = 1;
const int kMenuBorderWidthTop = 1;
const int kMenuBorderWidthRight = 1;

// Limit how small a combobox can be.
const int kMinComboboxWidth = 25;

// Define the id of the first item in the menu (since it needs to be > 0)
const int kFirstMenuItemId = 1000;

// Used to indicate that no item is currently selected by the user.
const int kNoSelection = -1;

const int kBodyButtonImages[] = IMAGE_GRID(IDR_COMBOBOX_BUTTON);
const int kHoveredBodyButtonImages[] = IMAGE_GRID(IDR_COMBOBOX_BUTTON_H);
const int kPressedBodyButtonImages[] = IMAGE_GRID(IDR_COMBOBOX_BUTTON_P);
const int kFocusedBodyButtonImages[] = IMAGE_GRID(IDR_COMBOBOX_BUTTON_F);
const int kFocusedHoveredBodyButtonImages[] =
    IMAGE_GRID(IDR_COMBOBOX_BUTTON_F_H);
const int kFocusedPressedBodyButtonImages[] =
    IMAGE_GRID(IDR_COMBOBOX_BUTTON_F_P);

#define MENU_IMAGE_GRID(x) { \
    x ## _MENU_TOP, x ## _MENU_CENTER, x ## _MENU_BOTTOM, }

const int kMenuButtonImages[] = MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON);
const int kHoveredMenuButtonImages[] = MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON_H);
const int kPressedMenuButtonImages[] = MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON_P);
const int kFocusedMenuButtonImages[] = MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON_F);
const int kFocusedHoveredMenuButtonImages[] =
    MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON_F_H);
const int kFocusedPressedMenuButtonImages[] =
    MENU_IMAGE_GRID(IDR_COMBOBOX_BUTTON_F_P);

#undef MENU_IMAGE_GRID

gfx::Rect PositionArrowWithinContainer(const gfx::Rect& container_bounds,
                                       const gfx::Size& arrow_size,
                                       Combobox::Style style) {
  gfx::Rect bounds(container_bounds);
  if (style == Combobox::STYLE_ACTION) {
    // This positions the arrow horizontally. The later call to
    // ClampToCenteredSize will position it vertically without touching the
    // horizontal position.
    bounds.Inset(kActionLeftPadding, 0, kActionRightPadding, 0);
    DCHECK_EQ(bounds.width(), arrow_size.width());
  }

  bounds.ClampToCenteredSize(arrow_size);
  return bounds;
}

// The transparent button which holds a button state but is not rendered.
class TransparentButton : public CustomButton {
 public:
  TransparentButton(ButtonListener* listener)
      : CustomButton(listener) {
    SetAnimationDuration(LabelButton::kHoverAnimationDurationMs);
    SetFocusBehavior(FocusBehavior::NEVER);
  }
  ~TransparentButton() override {}

  bool OnMousePressed(const ui::MouseEvent& mouse_event) override {
    parent()->RequestFocus();
    return true;
  }

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TransparentButton);
};

// Returns the next or previous valid index (depending on |increment|'s value).
// Skips separator or disabled indices. Returns -1 if there is no valid adjacent
// index.
int GetAdjacentIndex(ui::ComboboxModel* model, int increment, int index) {
  DCHECK(increment == -1 || increment == 1);

  index += increment;
  while (index >= 0 && index < model->GetItemCount()) {
    if (!model->IsItemSeparatorAt(index) || !model->IsItemEnabledAt(index))
      return index;
    index += increment;
  }
  return kNoSelection;
}

// Returns the image resource ids of an array for the body button.
//
// TODO(hajimehoshi): This function should return the images for the 'disabled'
// status. (crbug/270052)
const int* GetBodyButtonImageIds(bool focused,
                                 Button::ButtonState state,
                                 size_t* num) {
  DCHECK(num);
  *num = 9;
  switch (state) {
    case Button::STATE_DISABLED:
      return focused ? kFocusedBodyButtonImages : kBodyButtonImages;
    case Button::STATE_NORMAL:
      return focused ? kFocusedBodyButtonImages : kBodyButtonImages;
    case Button::STATE_HOVERED:
      return focused ?
          kFocusedHoveredBodyButtonImages : kHoveredBodyButtonImages;
    case Button::STATE_PRESSED:
      return focused ?
          kFocusedPressedBodyButtonImages : kPressedBodyButtonImages;
    default:
      NOTREACHED();
  }
  return NULL;
}

// Returns the image resource ids of an array for the menu button.
const int* GetMenuButtonImageIds(bool focused,
                                 Button::ButtonState state,
                                 size_t* num) {
  DCHECK(num);
  *num = 3;
  switch (state) {
    case Button::STATE_DISABLED:
      return focused ? kFocusedMenuButtonImages : kMenuButtonImages;
    case Button::STATE_NORMAL:
      return focused ? kFocusedMenuButtonImages : kMenuButtonImages;
    case Button::STATE_HOVERED:
      return focused ?
          kFocusedHoveredMenuButtonImages : kHoveredMenuButtonImages;
    case Button::STATE_PRESSED:
      return focused ?
          kFocusedPressedMenuButtonImages : kPressedMenuButtonImages;
    default:
      NOTREACHED();
  }
  return NULL;
}

// Returns the images for the menu buttons.
std::vector<const gfx::ImageSkia*> GetMenuButtonImages(
    bool focused,
    Button::ButtonState state) {
  const int* ids;
  size_t num_ids;
  ids = GetMenuButtonImageIds(focused, state, &num_ids);
  std::vector<const gfx::ImageSkia*> images;
  images.reserve(num_ids);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (size_t i = 0; i < num_ids; i++)
    images.push_back(rb.GetImageSkiaNamed(ids[i]));
  return images;
}

// Paints three images in a column at the given location. The center image is
// stretched so as to fit the given height.
void PaintImagesVertically(gfx::Canvas* canvas,
                           const gfx::ImageSkia& top_image,
                           const gfx::ImageSkia& center_image,
                           const gfx::ImageSkia& bottom_image,
                           int x, int y, int width, int height) {
  canvas->DrawImageInt(top_image,
                       0, 0, top_image.width(), top_image.height(),
                       x, y, width, top_image.height(), false);
  y += top_image.height();
  int center_height = height - top_image.height() - bottom_image.height();
  canvas->DrawImageInt(center_image,
                       0, 0, center_image.width(), center_image.height(),
                       x, y, width, center_height, false);
  y += center_height;
  canvas->DrawImageInt(bottom_image,
                       0, 0, bottom_image.width(), bottom_image.height(),
                       x, y, width, bottom_image.height(), false);
}

// Paints the arrow button.
void PaintArrowButton(
    gfx::Canvas* canvas,
    const std::vector<const gfx::ImageSkia*>& arrow_button_images,
    int x, int height) {
  PaintImagesVertically(canvas,
                        *arrow_button_images[0],
                        *arrow_button_images[1],
                        *arrow_button_images[2],
                        x, 0, arrow_button_images[0]->width(), height);
}

}  // namespace

// static
const char Combobox::kViewClassName[] = "views/Combobox";

// Adapts a ui::ComboboxModel to a ui::MenuModel.
class Combobox::ComboboxMenuModelAdapter : public ui::MenuModel,
                                           public ui::ComboboxModelObserver {
 public:
  ComboboxMenuModelAdapter(Combobox* owner, ui::ComboboxModel* model)
      : owner_(owner), model_(model) {
    model_->AddObserver(this);
  }

  ~ComboboxMenuModelAdapter() override { model_->RemoveObserver(this); }

 private:
  bool UseCheckmarks() const {
    return owner_->style_ != STYLE_ACTION &&
           MenuConfig::instance().check_selected_combobox_item;
  }

  // Overridden from MenuModel:
  bool HasIcons() const override { return false; }

  int GetItemCount() const override { return model_->GetItemCount(); }

  ItemType GetTypeAt(int index) const override {
    if (model_->IsItemSeparatorAt(index)) {
      // In action menus, disallow <item>, <separator>, ... since that would put
      // a separator at the top of the menu.
      DCHECK(index != 1 || owner_->style_ != STYLE_ACTION);
      return TYPE_SEPARATOR;
    }
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    return index + kFirstMenuItemId;
  }

  base::string16 GetLabelAt(int index) const override {
    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    base::string16 text = model_->GetItemAt(index);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  bool IsItemDynamicAt(int index) const override { return true; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return UseCheckmarks() && index == owner_->selected_index_;
  }

  int GetGroupIdAt(int index) const override { return -1; }

  bool GetIconAt(int index, gfx::Image* icon) override { return false; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return model_->IsItemEnabledAt(index);
  }

  bool IsVisibleAt(int index) const override {
    // When STYLE_ACTION is used, the first item is not added to the menu. It is
    // assumed that the first item is always selected and rendered on the top of
    // the action button.
    return index > 0 || owner_->style_ != STYLE_ACTION;
  }

  void HighlightChangedTo(int index) override {}

  void ActivatedAt(int index) override {
    owner_->selected_index_ = index;
    owner_->OnPerformAction();
  }

  void ActivatedAt(int index, int event_flags) override { ActivatedAt(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  void SetMenuModelDelegate(
      ui::MenuModelDelegate* menu_model_delegate) override {}

  ui::MenuModelDelegate* GetMenuModelDelegate() const override {
    return nullptr;
  }

  // Overridden from ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    owner_->ModelChanged();
  }

  Combobox* owner_;           // Weak. Owns this.
  ui::ComboboxModel* model_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(ComboboxMenuModelAdapter);
};

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(ui::ComboboxModel* model, Style style)
    : model_(model),
      style_(style),
      listener_(NULL),
      selected_index_(style == STYLE_ACTION ? 0 : model_->GetDefaultIndex()),
      invalid_(false),
      menu_model_adapter_(new ComboboxMenuModelAdapter(this, model)),
      text_button_(new TransparentButton(this)),
      arrow_button_(new TransparentButton(this)),
      size_to_largest_label_(style_ == STYLE_NORMAL),
      weak_ptr_factory_(this) {
  ModelChanged();
#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  UpdateBorder();
  arrow_image_ = PlatformStyle::CreateComboboxArrow(enabled(), style);
  // set_background() takes ownership but takes a raw pointer.
  std::unique_ptr<Background> b =
      PlatformStyle::CreateComboboxBackground(GetArrowContainerWidth());
  set_background(b.release());

  // Initialize the button images.
  Button::ButtonState button_states[] = {
    Button::STATE_DISABLED,
    Button::STATE_NORMAL,
    Button::STATE_HOVERED,
    Button::STATE_PRESSED,
  };
  for (int i = 0; i < 2; i++) {
    for (size_t state_index = 0; state_index < arraysize(button_states);
         state_index++) {
      Button::ButtonState state = button_states[state_index];
      size_t num;
      bool focused = !!i;
      const int* ids = GetBodyButtonImageIds(focused, state, &num);
      body_button_painters_[focused][state].reset(
          Painter::CreateImageGridPainter(ids));
      menu_button_images_[focused][state] = GetMenuButtonImages(focused, state);
    }
  }

  text_button_->SetVisible(true);
  arrow_button_->SetVisible(true);
  AddChildView(text_button_);
  AddChildView(arrow_button_);
}

Combobox::~Combobox() {
  if (GetInputMethod() && selector_.get()) {
    // Combobox should have been blurred before destroy.
    DCHECK(selector_.get() != GetInputMethod()->GetTextInputClient());
  }
}

// static
const gfx::FontList& Combobox::GetFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontListWithDelta(ui::kLabelFontSizeDelta);
}

void Combobox::ModelChanged() {
  // If the selection is no longer valid (or the model is empty), restore the
  // default index.
  if (selected_index_ >= model_->GetItemCount() ||
      model_->GetItemCount() == 0 ||
      model_->IsItemSeparatorAt(selected_index_)) {
    selected_index_ = model_->GetDefaultIndex();
  }

  content_size_ = GetContentSize();
  PreferredSizeChanged();
}

void Combobox::SetSelectedIndex(int index) {
  if (style_ == STYLE_ACTION)
    return;

  selected_index_ = index;
  if (size_to_largest_label_) {
    SchedulePaint();
  } else {
    content_size_ = GetContentSize();
    PreferredSizeChanged();
  }
}

bool Combobox::SelectValue(const base::string16& value) {
  if (style_ == STYLE_ACTION)
    return false;

  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (value == model()->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

void Combobox::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void Combobox::SetInvalid(bool invalid) {
  if (invalid == invalid_)
    return;

  invalid_ = invalid;

  UpdateBorder();
  SchedulePaint();
}

void Combobox::Layout() {
  PrefixDelegate::Layout();

  int text_button_width = 0;
  int arrow_button_width = 0;

  switch (style_) {
    case STYLE_NORMAL: {
      arrow_button_width = width();
      break;
    }
    case STYLE_ACTION: {
      arrow_button_width = GetArrowContainerWidth();
      text_button_width = width() - arrow_button_width;
      break;
    }
  }

  int arrow_button_x = std::max(0, text_button_width);
  text_button_->SetBounds(0, 0, std::max(0, text_button_width), height());
  arrow_button_->SetBounds(arrow_button_x, 0, arrow_button_width, height());
}

void Combobox::OnEnabledChanged() {
  PrefixDelegate::OnEnabledChanged();
  arrow_image_ = PlatformStyle::CreateComboboxArrow(enabled(), style_);
}

int Combobox::GetRowCount() {
  return model()->GetItemCount();
}

int Combobox::GetSelectedRow() {
  return selected_index_;
}

void Combobox::SetSelectedRow(int row) {
  int prev_index = selected_index_;
  SetSelectedIndex(row);
  if (selected_index_ != prev_index)
    OnPerformAction();
}

base::string16 Combobox::GetTextForRow(int row) {
  return model()->IsItemSeparatorAt(row) ? base::string16() :
                                           model()->GetItemAt(row);
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::GetPreferredSize() const {
  // The preferred size will drive the local bounds which in turn is used to set
  // the minimum width for the dropdown list.
  gfx::Insets insets = GetInsets();
  insets += gfx::Insets(Textfield::kTextPadding,
                        Textfield::kTextPadding,
                        Textfield::kTextPadding,
                        Textfield::kTextPadding);
  int total_width = std::max(kMinComboboxWidth, content_size_.width()) +
                    insets.width() + GetArrowContainerWidth();
  return gfx::Size(total_width, content_size_.height() + insets.height());
}

const char* Combobox::GetClassName() const {
  return kViewClassName;
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() != ui::VKEY_ESCAPE ||
      e.IsShiftDown() || e.IsControlDown() || e.IsAltDown()) {
    return false;
  }
  return !!menu_runner_;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  // TODO(oshima): handle IME.
  DCHECK_EQ(e.type(), ui::ET_KEY_PRESSED);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;

  bool show_menu = false;
  int new_index = kNoSelection;
  switch (e.key_code()) {
    // Show the menu on F4 without modifiers.
    case ui::VKEY_F4:
      if (e.IsAltDown() || e.IsAltGrDown() || e.IsControlDown())
        return false;
      show_menu = true;
      break;

    // Move to the next item if any, or show the menu on Alt+Down like Windows.
    case ui::VKEY_DOWN:
      if (e.IsAltDown())
        show_menu = true;
      else
        new_index = GetAdjacentIndex(model(), 1, selected_index_);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = GetAdjacentIndex(model(), -1, model()->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = GetAdjacentIndex(model(), 1, -1);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = GetAdjacentIndex(model(), -1, selected_index_);
      break;

    // Click the button only when the button style mode.
    case ui::VKEY_SPACE:
      if (style_ == STYLE_ACTION) {
        // When pressing space, the click event will be raised after the key is
        // released.
        text_button_->SetState(Button::STATE_PRESSED);
      } else {
        return false;
      }
      break;

    // Click the button only when the button style mode.
    case ui::VKEY_RETURN:
      if (style_ != STYLE_ACTION)
        return false;
      OnPerformAction();
      break;

    default:
      return false;
  }

  if (show_menu) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
  } else if (new_index != selected_index_ && new_index != kNoSelection &&
             style_ != STYLE_ACTION) {
    DCHECK(!model()->IsItemSeparatorAt(new_index));
    selected_index_ = new_index;
    OnPerformAction();
  }

  return true;
}

bool Combobox::OnKeyReleased(const ui::KeyEvent& e) {
  if (style_ != STYLE_ACTION)
    return false;  // crbug.com/127520

  if (e.key_code() == ui::VKEY_SPACE && style_ == STYLE_ACTION)
    OnPerformAction();

  return false;
}

void Combobox::OnPaint(gfx::Canvas* canvas) {
  switch (style_) {
    case STYLE_NORMAL: {
      OnPaintBackground(canvas);
      PaintText(canvas);
      OnPaintBorder(canvas);
      break;
    }
    case STYLE_ACTION: {
      PaintButtons(canvas);
      PaintText(canvas);
      break;
    }
  }
}

void Combobox::OnFocus() {
  if (GetInputMethod())
    GetInputMethod()->SetFocusedTextInputClient(GetPrefixSelector());

  View::OnFocus();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::OnBlur() {
  if (GetInputMethod())
    GetInputMethod()->DetachTextInputClient(GetPrefixSelector());

  if (selector_)
    selector_->OnViewBlur();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_COMBO_BOX;
  state->name = accessible_name_;
  state->value = model_->GetItemAt(selected_index_);
  state->index = selected_index_;
  state->count = model_->GetItemCount();
}

void Combobox::ButtonPressed(Button* sender, const ui::Event& event) {
  if (!enabled())
    return;

  RequestFocus();

  if (sender == text_button_) {
    OnPerformAction();
  } else {
    DCHECK_EQ(arrow_button_, sender);
    // TODO(hajimehoshi): Fix the problem that the arrow button blinks when
    // cliking this while the dropdown menu is opened.
    const base::TimeDelta delta = base::Time::Now() - closed_time_;
    if (delta.InMilliseconds() <= kMinimumMsBetweenButtonClicks)
      return;

    ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
    if (event.IsKeyEvent())
      source_type = ui::MENU_SOURCE_KEYBOARD;
    else if (event.IsGestureEvent() || event.IsTouchEvent())
      source_type = ui::MENU_SOURCE_TOUCH;
    ShowDropDownMenu(source_type);
  }
}

void Combobox::UpdateBorder() {
  std::unique_ptr<FocusableBorder> border(
      PlatformStyle::CreateComboboxBorder());
  if (style_ == STYLE_ACTION)
    border->SetInsets(5, 10, 5, 10);
  if (invalid_)
    border->SetColor(gfx::kGoogleRed700);
  SetBorder(std::move(border));
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();
  insets += gfx::Insets(0, Textfield::kTextPadding, 0, Textfield::kTextPadding);

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor text_color = GetNativeTheme()->GetSystemColor(
      enabled() ? ui::NativeTheme::kColorId_LabelEnabledColor :
                  ui::NativeTheme::kColorId_LabelDisabledColor);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;
  base::string16 text = model()->GetItemAt(selected_index_);

  int disclosure_arrow_offset = width() - GetArrowContainerWidth();

  const gfx::FontList& font_list = Combobox::GetFontList();
  int text_width = gfx::GetStringWidth(text, font_list);
  if ((text_width + insets.width()) > disclosure_arrow_offset)
    text_width = disclosure_arrow_offset - insets.width();

  gfx::Rect text_bounds(x, y, text_width, text_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringRect(text, font_list, text_color, text_bounds);

  gfx::Rect arrow_bounds(disclosure_arrow_offset, 0, GetArrowContainerWidth(),
                         height());
  arrow_bounds =
      PositionArrowWithinContainer(arrow_bounds, ArrowSize(), style_);
  AdjustBoundsForRTLUI(&arrow_bounds);

  canvas->DrawImageInt(arrow_image_, arrow_bounds.x(), arrow_bounds.y());
}

void Combobox::PaintButtons(gfx::Canvas* canvas) {
  DCHECK(style_ == STYLE_ACTION);

  gfx::ScopedRTLFlipCanvas scoped_canvas(canvas, bounds());

  bool focused = HasFocus();
  const std::vector<const gfx::ImageSkia*>& arrow_button_images =
      menu_button_images_[focused][
          arrow_button_->state() == Button::STATE_HOVERED ?
          Button::STATE_NORMAL : arrow_button_->state()];

  int text_button_hover_alpha =
      text_button_->state() == Button::STATE_PRESSED ? 0 :
      static_cast<int>(static_cast<TransparentButton*>(text_button_)->
                       GetAnimationValue() * 255);
  if (text_button_hover_alpha < 255) {
    canvas->SaveLayerAlpha(255 - text_button_hover_alpha);
    Painter* text_button_painter =
        body_button_painters_[focused][
            text_button_->state() == Button::STATE_HOVERED ?
            Button::STATE_NORMAL : text_button_->state()].get();
    Painter::PaintPainterAt(canvas, text_button_painter,
                            gfx::Rect(0, 0, text_button_->width(), height()));
    canvas->Restore();
  }
  if (0 < text_button_hover_alpha) {
    canvas->SaveLayerAlpha(text_button_hover_alpha);
    Painter* text_button_hovered_painter =
        body_button_painters_[focused][Button::STATE_HOVERED].get();
    Painter::PaintPainterAt(canvas, text_button_hovered_painter,
                            gfx::Rect(0, 0, text_button_->width(), height()));
    canvas->Restore();
  }

  int arrow_button_hover_alpha =
      arrow_button_->state() == Button::STATE_PRESSED ? 0 :
      static_cast<int>(static_cast<TransparentButton*>(arrow_button_)->
                       GetAnimationValue() * 255);
  if (arrow_button_hover_alpha < 255) {
    canvas->SaveLayerAlpha(255 - arrow_button_hover_alpha);
    PaintArrowButton(canvas, arrow_button_images, arrow_button_->x(), height());
    canvas->Restore();
  }
  if (0 < arrow_button_hover_alpha) {
    canvas->SaveLayerAlpha(arrow_button_hover_alpha);
    const std::vector<const gfx::ImageSkia*>& arrow_button_hovered_images =
        menu_button_images_[focused][Button::STATE_HOVERED];
    PaintArrowButton(canvas, arrow_button_hovered_images,
                     arrow_button_->x(), height());
    canvas->Restore();
  }
}

void Combobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  gfx::Rect lb = GetLocalBounds();
  gfx::Point menu_position(lb.origin());

  if (style_ == STYLE_NORMAL) {
    // Inset the menu's requested position so the border of the menu lines up
    // with the border of the combobox.
    menu_position.set_x(menu_position.x() + kMenuBorderWidthLeft);
    menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  }
  lb.set_width(lb.width() - (kMenuBorderWidthLeft + kMenuBorderWidthRight));

  View::ConvertPointToScreen(this, &menu_position);

  gfx::Rect bounds(menu_position, lb.size());

  Button::ButtonState original_state = Button::STATE_NORMAL;
  if (arrow_button_) {
    original_state = arrow_button_->state();
    arrow_button_->SetState(Button::STATE_PRESSED);
  }
  MenuAnchorPosition anchor_position =
      style_ == STYLE_ACTION ? MENU_ANCHOR_TOPRIGHT : MENU_ANCHOR_TOPLEFT;

  // Allow |menu_runner_| to be set by the testing API, but if this method is
  // ever invoked recursively, ensure the old menu is closed.
  if (!menu_runner_ || menu_runner_->IsRunning()) {
    menu_runner_.reset(
        new MenuRunner(menu_model_adapter_.get(), MenuRunner::COMBOBOX));
  }
  if (menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds, anchor_position,
                              source_type) == MenuRunner::MENU_DELETED) {
    return;
  }
  menu_runner_.reset();
  if (arrow_button_)
    arrow_button_->SetState(original_state);
  closed_time_ = base::Time::Now();

  // Need to explicitly clear mouse handler so that events get sent
  // properly after the menu finishes running. If we don't do this, then
  // the first click to other parts of the UI is eaten.
  SetMouseHandler(NULL);
}

void Combobox::OnPerformAction() {
  NotifyAccessibilityEvent(ui::AX_EVENT_VALUE_CHANGED, false);
  SchedulePaint();

  // This combobox may be deleted by the listener.
  base::WeakPtr<Combobox> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  if (listener_)
    listener_->OnPerformAction(this);

  if (weak_ptr && style_ == STYLE_ACTION)
    selected_index_ = 0;
}

gfx::Size Combobox::ArrowSize() const {
  return arrow_image_.size();
}

gfx::Size Combobox::GetContentSize() const {
  const gfx::FontList& font_list = GetFontList();

  int width = 0;
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (model_->IsItemSeparatorAt(i))
      continue;

    if (size_to_largest_label_ || i == selected_index_) {
      width = std::max(
          width,
          gfx::GetStringWidth(menu_model_adapter_->GetLabelAt(i), font_list));
    }
  }
  return gfx::Size(width, font_list.GetHeight());
}

PrefixSelector* Combobox::GetPrefixSelector() {
  if (!selector_)
    selector_.reset(new PrefixSelector(this));
  return selector_.get();
}

int Combobox::GetArrowContainerWidth() const {
  int padding = style_ == STYLE_NORMAL
                    ? PlatformStyle::kComboboxNormalArrowPadding * 2
                    : kActionLeftPadding + kActionRightPadding;
  return ArrowSize().width() + padding;
}

}  // namespace views
