// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/color_constants.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Menu border widths
const int kMenuBorderWidthLeft = 1;
const int kMenuBorderWidthTop = 1;
const int kMenuBorderWidthRight = 1;

// Limit how small a combobox can be.
const int kMinComboboxWidth = 25;

// Size of the combobox arrow margins
const int kDisclosureArrowLeftPadding = 7;
const int kDisclosureArrowRightPadding = 7;
const int kDisclosureArrowButtonLeftPadding = 11;
const int kDisclosureArrowButtonRightPadding = 12;

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

// The transparent button which holds a button state but is not rendered.
class TransparentButton : public CustomButton {
 public:
  TransparentButton(ButtonListener* listener)
      : CustomButton(listener) {
    SetAnimationDuration(LabelButton::kHoverAnimationDurationMs);
  }
  virtual ~TransparentButton() {}

  virtual bool OnMousePressed(const ui::MouseEvent& mouse_event) OVERRIDE {
    parent()->RequestFocus();
    return true;
  }

  double GetAnimationValue() const {
    return hover_animation_->GetCurrentValue();
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

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(ui::ComboboxModel* model)
    : model_(model),
      style_(STYLE_NORMAL),
      listener_(NULL),
      selected_index_(model_->GetDefaultIndex()),
      invalid_(false),
      dropdown_open_(false),
      text_button_(new TransparentButton(this)),
      arrow_button_(new TransparentButton(this)),
      weak_ptr_factory_(this) {
  model_->AddObserver(this);
  UpdateFromModel();
  SetFocusable(true);
  UpdateBorder();

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
  text_button_->SetFocusable(false);
  arrow_button_->SetFocusable(false);
  AddChildView(text_button_);
  AddChildView(arrow_button_);
}

Combobox::~Combobox() {
  model_->RemoveObserver(this);
}

// static
const gfx::FontList& Combobox::GetFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::BaseFont);
}

void Combobox::SetStyle(Style style) {
  if (style_ == style)
    return;

  style_ = style;
  if (style_ == STYLE_ACTION)
    selected_index_ = 0;

  UpdateBorder();
  UpdateFromModel();
  PreferredSizeChanged();
}

void Combobox::ModelChanged() {
  selected_index_ = std::min(0, model_->GetItemCount());
  UpdateFromModel();
  PreferredSizeChanged();
}

void Combobox::SetSelectedIndex(int index) {
  if (style_ == STYLE_ACTION)
    return;

  selected_index_ = index;
  SchedulePaint();
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

ui::TextInputClient* Combobox::GetTextInputClient() {
  if (!selector_)
    selector_.reset(new PrefixSelector(this));
  return selector_.get();
}

void Combobox::Layout() {
  PrefixDelegate::Layout();

  gfx::Insets insets = GetInsets();
  int text_button_width = 0;
  int arrow_button_width = 0;

  switch (style_) {
    case STYLE_NORMAL: {
      arrow_button_width = width();
      break;
    }
    case STYLE_ACTION: {
      arrow_button_width = GetDisclosureArrowLeftPadding() +
          ArrowSize().width() +
          GetDisclosureArrowRightPadding();
      text_button_width = width() - arrow_button_width;
      break;
    }
  }

  int arrow_button_x = std::max(0, text_button_width);
  text_button_->SetBounds(0, 0, std::max(0, text_button_width), height());
  arrow_button_->SetBounds(arrow_button_x, 0, arrow_button_width, height());
}

bool Combobox::IsItemChecked(int id) const {
  return false;
}

bool Combobox::IsCommandEnabled(int id) const {
  return model()->IsItemEnabledAt(MenuCommandToIndex(id));
}

void Combobox::ExecuteCommand(int id) {
  selected_index_ = MenuCommandToIndex(id);
  OnPerformAction();
}

bool Combobox::GetAccelerator(int id, ui::Accelerator* accel) const {
  return false;
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
  int total_width = std::max(kMinComboboxWidth, content_size_.width()) +
      insets.width() + GetDisclosureArrowLeftPadding() +
      ArrowSize().width() + GetDisclosureArrowRightPadding();
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
  return dropdown_open_;
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
    UpdateFromModel();
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
  GetInputMethod()->OnFocus();
  View::OnFocus();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::OnBlur() {
  GetInputMethod()->OnBlur();
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

void Combobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  DCHECK_EQ(model, model_);
  ModelChanged();
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

void Combobox::UpdateFromModel() {
  const gfx::FontList& font_list = Combobox::GetFontList();

  MenuItemView* menu = new MenuItemView(this);
  // MenuRunner owns |menu|.
  dropdown_list_menu_runner_.reset(new MenuRunner(menu));

  int num_items = model()->GetItemCount();
  int width = 0;
  bool text_item_appended = false;
  for (int i = 0; i < num_items; ++i) {
    // When STYLE_ACTION is used, the first item and the following separators
    // are not added to the dropdown menu. It is assumed that the first item is
    // always selected and rendered on the top of the action button.
    if (model()->IsItemSeparatorAt(i)) {
      if (text_item_appended || style_ != STYLE_ACTION)
        menu->AppendSeparator();
      continue;
    }

    base::string16 text = model()->GetItemAt(i);

    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    base::i18n::AdjustStringForLocaleDirection(&text);

    if (style_ != STYLE_ACTION || i > 0) {
      menu->AppendMenuItem(i + kFirstMenuItemId, text, MenuItemView::NORMAL);
      text_item_appended = true;
    }

    if (style_ != STYLE_ACTION || i == selected_index_)
      width = std::max(width, gfx::GetStringWidth(text, font_list));
  }

  content_size_.SetSize(width, font_list.GetHeight());
}

void Combobox::UpdateBorder() {
  scoped_ptr<FocusableBorder> border(new FocusableBorder());
  if (style_ == STYLE_ACTION)
    border->SetInsets(8, 13, 8, 13);
  if (invalid_)
    border->SetColor(kWarningColor);
  SetBorder(border.PassAs<Border>());
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;
  base::string16 text = model()->GetItemAt(selected_index_);

  gfx::Size arrow_size = ArrowSize();
  int disclosure_arrow_offset = width() - arrow_size.width() -
      GetDisclosureArrowLeftPadding() - GetDisclosureArrowRightPadding();

  const gfx::FontList& font_list = Combobox::GetFontList();
  int text_width = gfx::GetStringWidth(text, font_list);
  if ((text_width + insets.width()) > disclosure_arrow_offset)
    text_width = disclosure_arrow_offset - insets.width();

  gfx::Rect text_bounds(x, y, text_width, text_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringRect(text, font_list, text_color, text_bounds);

  int arrow_x = disclosure_arrow_offset + GetDisclosureArrowLeftPadding();
  gfx::Rect arrow_bounds(arrow_x,
                         height() / 2 - arrow_size.height() / 2,
                         arrow_size.width(),
                         arrow_size.height());
  AdjustBoundsForRTLUI(&arrow_bounds);

  // TODO(estade): hack alert! Remove this direct call into CommonTheme. For now
  // STYLE_ACTION isn't properly themed so we have to override the NativeTheme
  // behavior. See crbug.com/384071
  if (style_ == STYLE_ACTION) {
    ui::CommonThemePaintComboboxArrow(canvas->sk_canvas(), arrow_bounds);
  } else {
    ui::NativeTheme::ExtraParams ignored;
    GetNativeTheme()->Paint(canvas->sk_canvas(),
                            ui::NativeTheme::kComboboxArrow,
                            ui::NativeTheme::kNormal,
                            arrow_bounds,
                            ignored);
  }
}

void Combobox::PaintButtons(gfx::Canvas* canvas) {
  DCHECK(style_ == STYLE_ACTION);

  gfx::ScopedCanvas scoped_canvas(canvas);
  if (base::i18n::IsRTL()) {
    canvas->Translate(gfx::Vector2d(width(), 0));
    canvas->Scale(-1, 1);
  }

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
  if (!dropdown_list_menu_runner_.get())
    UpdateFromModel();

  // Extend the menu to the width of the combobox.
  MenuItemView* menu = dropdown_list_menu_runner_->GetMenu();
  SubmenuView* submenu = menu->CreateSubmenu();
  submenu->set_minimum_preferred_width(
      size().width() - (kMenuBorderWidthLeft + kMenuBorderWidthRight));

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
  if (menu_position.x() < 0)
    menu_position.set_x(0);

  gfx::Rect bounds(menu_position, lb.size());

  Button::ButtonState original_state = Button::STATE_NORMAL;
  if (arrow_button_) {
    original_state = arrow_button_->state();
    arrow_button_->SetState(Button::STATE_PRESSED);
  }
  dropdown_open_ = true;
  MenuAnchorPosition anchor_position =
      style_ == STYLE_ACTION ? MENU_ANCHOR_TOPRIGHT : MENU_ANCHOR_TOPLEFT;
  if (dropdown_list_menu_runner_->RunMenuAt(GetWidget(), NULL, bounds,
                                            anchor_position, source_type,
                                            MenuRunner::COMBOBOX) ==
      MenuRunner::MENU_DELETED) {
    return;
  }
  dropdown_open_ = false;
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

int Combobox::MenuCommandToIndex(int menu_command_id) const {
  // (note that the id received is offset by kFirstMenuItemId)
  // Revert menu ID offset to map back to combobox model.
  int index = menu_command_id - kFirstMenuItemId;
  DCHECK_LT(index, model()->GetItemCount());
  return index;
}

int Combobox::GetDisclosureArrowLeftPadding() const {
  switch (style_) {
    case STYLE_NORMAL:
      return kDisclosureArrowLeftPadding;
    case STYLE_ACTION:
      return kDisclosureArrowButtonLeftPadding;
  }
  NOTREACHED();
  return 0;
}

int Combobox::GetDisclosureArrowRightPadding() const {
  switch (style_) {
    case STYLE_NORMAL:
      return kDisclosureArrowRightPadding;
    case STYLE_ACTION:
      return kDisclosureArrowButtonRightPadding;
  }
  NOTREACHED();
  return 0;
}

gfx::Size Combobox::ArrowSize() const {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // TODO(estade): hack alert! This should always use GetNativeTheme(). For now
  // STYLE_ACTION isn't properly themed so we have to override the NativeTheme
  // behavior. See crbug.com/384071
  const ui::NativeTheme* native_theme_for_arrow = style_ == STYLE_ACTION ?
      ui::NativeTheme::instance() :
      GetNativeTheme();
#else
  const ui::NativeTheme* native_theme_for_arrow = GetNativeTheme();
#endif

  ui::NativeTheme::ExtraParams ignored;
  return native_theme_for_arrow->GetPartSize(ui::NativeTheme::kComboboxArrow,
                                             ui::NativeTheme::kNormal,
                                             ignored);
}

}  // namespace views
