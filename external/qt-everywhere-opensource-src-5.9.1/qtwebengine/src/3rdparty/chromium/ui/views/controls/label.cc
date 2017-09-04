// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/native_cursor.h"
#include "ui/views/selection_controller.h"

namespace views {
// static
const char Label::kViewClassName[] = "Label";
const int Label::kFocusBorderPadding = 1;

Label::Label() : Label(base::string16()) {
}

Label::Label(const base::string16& text) : Label(text, GetDefaultFontList()) {
}

Label::Label(const base::string16& text, const gfx::FontList& font_list)
    : context_menu_contents_(this) {
  Init(text, font_list);
}

Label::~Label() {
}

// static
const gfx::FontList& Label::GetDefaultFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontListWithDelta(ui::kLabelFontSizeDelta);
}

void Label::SetFontList(const gfx::FontList& font_list) {
  is_first_paint_text_ = true;
  render_text_->SetFontList(font_list);
  ResetLayout();
}

void Label::SetText(const base::string16& new_text) {
  if (new_text == text())
    return;
  is_first_paint_text_ = true;
  render_text_->SetText(new_text);
  ResetLayout();
  stored_selection_range_ = gfx::Range::InvalidRange();
}

void Label::SetAutoColorReadabilityEnabled(bool enabled) {
  if (auto_color_readability_ == enabled)
    return;
  is_first_paint_text_ = true;
  auto_color_readability_ = enabled;
  RecalculateColors();
}

void Label::SetEnabledColor(SkColor color) {
  if (enabled_color_set_ && requested_enabled_color_ == color)
    return;
  is_first_paint_text_ = true;
  requested_enabled_color_ = color;
  enabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetDisabledColor(SkColor color) {
  if (disabled_color_set_ && requested_disabled_color_ == color)
    return;
  is_first_paint_text_ = true;
  requested_disabled_color_ = color;
  disabled_color_set_ = true;
  RecalculateColors();
}

void Label::SetBackgroundColor(SkColor color) {
  if (background_color_set_ && background_color_ == color)
    return;
  is_first_paint_text_ = true;
  background_color_ = color;
  background_color_set_ = true;
  RecalculateColors();
}

void Label::SetSelectionTextColor(SkColor color) {
  if (selection_text_color_set_ && requested_selection_text_color_ == color)
    return;
  is_first_paint_text_ = true;
  requested_selection_text_color_ = color;
  selection_text_color_set_ = true;
  RecalculateColors();
}

void Label::SetSelectionBackgroundColor(SkColor color) {
  if (selection_background_color_set_ && selection_background_color_ == color)
    return;
  is_first_paint_text_ = true;
  selection_background_color_ = color;
  selection_background_color_set_ = true;
  RecalculateColors();
}

void Label::SetShadows(const gfx::ShadowValues& shadows) {
  // TODO(mukai): early exit if the specified shadows are same.
  is_first_paint_text_ = true;
  render_text_->set_shadows(shadows);
  ResetLayout();
}

void Label::SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled) {
  if (subpixel_rendering_enabled_ == subpixel_rendering_enabled)
    return;
  is_first_paint_text_ = true;
  subpixel_rendering_enabled_ = subpixel_rendering_enabled;
  RecalculateColors();
}

void Label::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  // If the UI layout is right-to-left, flip the alignment direction.
  if (base::i18n::IsRTL() &&
      (alignment == gfx::ALIGN_LEFT || alignment == gfx::ALIGN_RIGHT)) {
    alignment = (alignment == gfx::ALIGN_LEFT) ?
        gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
  }
  if (horizontal_alignment() == alignment)
    return;
  is_first_paint_text_ = true;
  render_text_->SetHorizontalAlignment(alignment);
  ResetLayout();
}

void Label::SetLineHeight(int height) {
  if (line_height() == height)
    return;
  is_first_paint_text_ = true;
  render_text_->SetMinLineHeight(height);
  ResetLayout();
}

void Label::SetMultiLine(bool multi_line) {
  DCHECK(!multi_line || (elide_behavior_ == gfx::ELIDE_TAIL ||
                         elide_behavior_ == gfx::NO_ELIDE));
  if (this->multi_line() == multi_line)
    return;
  is_first_paint_text_ = true;
  multi_line_ = multi_line;
  if (render_text_->MultilineSupported())
    render_text_->SetMultiline(multi_line);
  render_text_->SetReplaceNewlineCharsWithSymbols(!multi_line);
  if (multi_line)
    SetSelectable(false);
  ResetLayout();
}

void Label::SetObscured(bool obscured) {
  if (this->obscured() == obscured)
    return;
  is_first_paint_text_ = true;
  render_text_->SetObscured(obscured);
  if (obscured)
    SetSelectable(false);
  ResetLayout();
}

void Label::SetAllowCharacterBreak(bool allow_character_break) {
  const gfx::WordWrapBehavior behavior =
      allow_character_break ? gfx::WRAP_LONG_WORDS : gfx::TRUNCATE_LONG_WORDS;
  if (render_text_->word_wrap_behavior() == behavior)
    return;
  render_text_->SetWordWrapBehavior(behavior);
  if (multi_line()) {
    is_first_paint_text_ = true;
    ResetLayout();
  }
}

void Label::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  DCHECK(!multi_line() || (elide_behavior_ == gfx::ELIDE_TAIL ||
                           elide_behavior_ == gfx::NO_ELIDE));
  if (elide_behavior_ == elide_behavior)
    return;
  is_first_paint_text_ = true;
  elide_behavior_ = elide_behavior;
  ResetLayout();
}

void Label::SetTooltipText(const base::string16& tooltip_text) {
  DCHECK(handles_tooltips_);
  tooltip_text_ = tooltip_text;
}

void Label::SetHandlesTooltips(bool enabled) {
  handles_tooltips_ = enabled;
}

void Label::SizeToFit(int fixed_width) {
  DCHECK(multi_line());
  DCHECK_EQ(0, max_width_);
  fixed_width_ = fixed_width;
  SizeToPreferredSize();
}

void Label::SetMaximumWidth(int max_width) {
  DCHECK(multi_line());
  DCHECK_EQ(0, fixed_width_);
  max_width_ = max_width;
  SizeToPreferredSize();
}

base::string16 Label::GetDisplayTextForTesting() {
  ClearRenderTextLines();
  MaybeBuildRenderTextLines();
  base::string16 result;
  if (lines_.empty())
    return result;
  result.append(lines_[0]->GetDisplayText());
  for (size_t i = 1; i < lines_.size(); ++i) {
    result.append(1, '\n');
    result.append(lines_[i]->GetDisplayText());
  }
  return result;
}

bool Label::IsSelectionSupported() const {
  return !multi_line() && !obscured() && render_text_->IsSelectionSupported();
}

bool Label::SetSelectable(bool value) {
  if (value == selectable())
    return true;

  if (!value) {
    ClearSelection();
    stored_selection_range_ = gfx::Range::InvalidRange();
    selection_controller_.reset();
    return true;
  }

  DCHECK(!stored_selection_range_.IsValid());
  if (!IsSelectionSupported())
    return false;

  selection_controller_ = base::MakeUnique<SelectionController>(this);
  return true;
}

bool Label::HasSelection() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? !render_text->selection().is_empty() : false;
}

void Label::SelectAll() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (!render_text)
    return;
  render_text->SelectAll(false);
  SchedulePaint();
}

void Label::ClearSelection() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (!render_text)
    return;
  render_text->ClearSelection();
  SchedulePaint();
}

void Label::SelectRange(const gfx::Range& range) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text && render_text->SelectRange(range))
    SchedulePaint();
}

gfx::Insets Label::GetInsets() const {
  gfx::Insets insets = View::GetInsets();
  if (focus_behavior() != FocusBehavior::NEVER) {
    insets += gfx::Insets(kFocusBorderPadding, kFocusBorderPadding,
                          kFocusBorderPadding, kFocusBorderPadding);
  }
  return insets;
}

int Label::GetBaseline() const {
  return GetInsets().top() + font_list().GetBaseline();
}

gfx::Size Label::GetPreferredSize() const {
  // Return a size of (0, 0) if the label is not visible and if the
  // |collapse_when_hidden_| flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need to respect the |collapse_when_hidden_| flag.
  if (!visible() && collapse_when_hidden_)
    return gfx::Size();

  if (multi_line() && fixed_width_ != 0 && !text().empty())
    return gfx::Size(fixed_width_, GetHeightForWidth(fixed_width_));

  gfx::Size size(GetTextSize());
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());

  if (multi_line() && max_width_ != 0 && max_width_ < size.width())
    return gfx::Size(max_width_, GetHeightForWidth(max_width_));

  return size;
}

gfx::Size Label::GetMinimumSize() const {
  if (!visible() && collapse_when_hidden_)
    return gfx::Size();

  gfx::Size size(0, font_list().GetHeight());
  if (elide_behavior_ == gfx::ELIDE_HEAD ||
      elide_behavior_ == gfx::ELIDE_MIDDLE ||
      elide_behavior_ == gfx::ELIDE_TAIL ||
      elide_behavior_ == gfx::ELIDE_EMAIL) {
    size.set_width(gfx::Canvas::GetStringWidth(
        base::string16(gfx::kEllipsisUTF16), font_list()));
  }
  if (!multi_line())
    size.SetToMin(GetTextSize());
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

int Label::GetHeightForWidth(int w) const {
  if (!visible() && collapse_when_hidden_)
    return 0;

  w -= GetInsets().width();
  int height = 0;
  if (!multi_line() || text().empty() || w <= 0) {
    height = std::max(line_height(), font_list().GetHeight());
  } else if (render_text_->MultilineSupported()) {
    // SetDisplayRect() has a side effect for later calls of GetStringSize().
    // Be careful to invoke |render_text_->SetDisplayRect(gfx::Rect())| to
    // cancel this effect before the next time GetStringSize() is called.
    // It would be beneficial not to cancel here, considering that some layout
    // managers invoke GetHeightForWidth() for the same width multiple times
    // and |render_text_| can cache the height.
    render_text_->SetDisplayRect(gfx::Rect(0, 0, w, 0));
    height = render_text_->GetStringSize().height();
  } else {
    std::vector<base::string16> lines = GetLinesForWidth(w);
    height = lines.size() * std::max(line_height(), font_list().GetHeight());
  }
  height -= gfx::ShadowValue::GetMargin(render_text_->shadows()).height();
  return height + GetInsets().height();
}

void Label::Layout() {
  ClearRenderTextLines();
}

const char* Label::GetClassName() const {
  return kViewClassName;
}

View* Label::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!handles_tooltips_ ||
      (tooltip_text_.empty() && !ShouldShowDefaultTooltip()))
    return NULL;

  return HitTestPoint(point) ? this : NULL;
}

bool Label::CanProcessEventsWithinSubtree() const {
  return !!GetRenderTextForSelectionController();
}

void Label::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_STATIC_TEXT;
  node_data->AddStateFlag(ui::AX_STATE_READ_ONLY);
  // Note that |render_text_| is never elided (see the comment in Init() too).
  node_data->SetName(render_text_->GetDisplayText());
}

bool Label::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  if (!handles_tooltips_)
    return false;

  if (!tooltip_text_.empty()) {
    tooltip->assign(tooltip_text_);
    return true;
  }

  if (ShouldShowDefaultTooltip()) {
    // Note that |render_text_| is never elided (see the comment in Init() too).
    tooltip->assign(render_text_->GetDisplayText());
    return true;
  }

  return false;
}

void Label::OnEnabledChanged() {
  ApplyTextColors();
  View::OnEnabledChanged();
}

std::unique_ptr<gfx::RenderText> Label::CreateRenderText(
    const base::string16& text,
    gfx::HorizontalAlignment alignment,
    gfx::DirectionalityMode directionality,
    gfx::ElideBehavior elide_behavior) const {
  std::unique_ptr<gfx::RenderText> render_text(
      render_text_->CreateInstanceOfSameType());
  render_text->SetHorizontalAlignment(alignment);
  render_text->SetDirectionalityMode(directionality);
  render_text->SetElideBehavior(elide_behavior);
  render_text->SetObscured(obscured());
  render_text->SetMinLineHeight(line_height());
  render_text->SetFontList(font_list());
  render_text->set_shadows(shadows());
  render_text->SetCursorEnabled(false);
  render_text->SetText(text);
  return render_text;
}

void Label::PaintText(gfx::Canvas* canvas) {
  MaybeBuildRenderTextLines();
  for (size_t i = 0; i < lines_.size(); ++i)
    lines_[i]->Draw(canvas);
}

void Label::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.size() != size())
    InvalidateLayout();
}

void Label::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (is_first_paint_text_) {
    // TODO(ckocagil): Remove ScopedTracker below once crbug.com/441028 is
    // fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("441028 First PaintText()"));

    is_first_paint_text_ = false;
    PaintText(canvas);
  } else {
    PaintText(canvas);
  }

  // Check for IsAccessibilityFocusable() to prevent drawing a focus rect for
  // non-focusable labels with selection, which are given focus explicitly in
  // OnMousePressed.
  if (HasFocus() && !ui::MaterialDesignController::IsSecondaryUiMaterial() &&
      IsAccessibilityFocusable()) {
    canvas->DrawFocusRect(GetFocusBounds());
  }
}

void Label::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

gfx::NativeCursor Label::GetCursor(const ui::MouseEvent& event) {
  return GetRenderTextForSelectionController() ? GetNativeIBeamCursor()
                                               : gfx::kNullCursor;
}

void Label::OnFocus() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text) {
    render_text->set_focused(true);
    SchedulePaint();
  }
  View::OnFocus();
}

void Label::OnBlur() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text) {
    render_text->set_focused(false);
    SchedulePaint();
  }
  View::OnBlur();
}

bool Label::OnMousePressed(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  // RequestFocus() won't work when the label has FocusBehavior::NEVER. Hence
  // explicitly set the focused view.
  // TODO(karandeepb): If a widget with a label having FocusBehavior::NEVER as
  // the currently focused view (due to selection) was to lose focus, focus
  // won't be restored to the label (and hence a text selection won't be drawn)
  // when the widget gets focus again. Fix this.
  // Tracked in https://crbug.com/630365.
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      GetFocusManager()) {
    GetFocusManager()->SetFocusedView(this);
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (event.IsOnlyMiddleMouseButton() && GetFocusManager())
    GetFocusManager()->SetFocusedView(this);
#endif

  return selection_controller_->OnMousePressed(event, false);
}

bool Label::OnMouseDragged(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  return selection_controller_->OnMouseDragged(event);
}

void Label::OnMouseReleased(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return;

  selection_controller_->OnMouseReleased(event);
}

void Label::OnMouseCaptureLost() {
  if (!GetRenderTextForSelectionController())
    return;

  selection_controller_->OnMouseCaptureLost();
}

bool Label::OnKeyPressed(const ui::KeyEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();

  switch (event.key_code()) {
    case ui::VKEY_C:
      if (control && !alt && HasSelection()) {
        CopyToClipboard();
        return true;
      }
      break;
    case ui::VKEY_INSERT:
      if (control && !shift && HasSelection()) {
        CopyToClipboard();
        return true;
      }
      break;
    case ui::VKEY_A:
      if (control && !alt && !text().empty()) {
        SelectAll();
        DCHECK(HasSelection());
        UpdateSelectionClipboard();
        return true;
      }
      break;
    default:
      break;
  }

  return false;
}

bool Label::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Allow the "Copy" action from the Chrome menu to be invoked. E.g., if a user
  // selects a Label on a web modal dialog. "Select All" doesn't appear in the
  // Chrome menu so isn't handled here.
  if (accelerator.key_code() == ui::VKEY_C && accelerator.IsCtrlDown()) {
    CopyToClipboard();
    return true;
  }
  return false;
}

bool Label::CanHandleAccelerators() const {
  // Focus needs to be checked since the accelerator for the Copy command from
  // the Chrome menu should only be handled when the current view has focus. See
  // related comment in BrowserView::CutCopyPaste.
  return HasFocus() && GetRenderTextForSelectionController() &&
         View::CanHandleAccelerators();
}

void Label::OnDeviceScaleFactorChanged(float device_scale_factor) {
  View::OnDeviceScaleFactorChanged(device_scale_factor);
  // When the device scale factor is changed, some font rendering parameters is
  // changed (especially, hinting). The bounding box of the text has to be
  // re-computed based on the new parameters. See crbug.com/441439
  ResetLayout();
}

void Label::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible)
    ClearRenderTextLines();
}

void Label::ShowContextMenuForView(View* source,
                                   const gfx::Point& point,
                                   ui::MenuSourceType source_type) {
  if (!GetRenderTextForSelectionController())
    return;

  context_menu_runner_.reset(
      new MenuRunner(&context_menu_contents_, MenuRunner::HAS_MNEMONICS |
                                                  MenuRunner::CONTEXT_MENU |
                                                  MenuRunner::ASYNC));
  ignore_result(context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()), MENU_ANCHOR_TOPLEFT,
      source_type));
}

gfx::RenderText* Label::GetRenderTextForSelectionController() {
  return const_cast<gfx::RenderText*>(
      static_cast<const Label*>(this)->GetRenderTextForSelectionController());
}

bool Label::IsReadOnly() const {
  return true;
}

bool Label::SupportsDrag() const {
  // TODO(crbug.com/661379): Labels should support dragging selected text.
  return false;
}

bool Label::HasTextBeingDragged() const {
  return false;
}

void Label::SetTextBeingDragged(bool value) {
  NOTREACHED();
}

int Label::GetViewHeight() const {
  return height();
}

int Label::GetViewWidth() const {
  return width();
}

int Label::GetDragSelectionDelay() const {
  // Labels don't need to use a repeating timer to update the drag selection.
  // Since the cursor is disabled for labels, a selection outside the display
  // area won't change the text in the display area. It is expected that all the
  // text will fit in the display area for labels anyway.
  return 0;
}

void Label::OnBeforePointerAction() {}

void Label::OnAfterPointerAction(bool text_changed, bool selection_changed) {
  DCHECK(!text_changed);
  if (selection_changed)
    SchedulePaint();
}

bool Label::PasteSelectionClipboard() {
  NOTREACHED();
  return false;
}

void Label::UpdateSelectionClipboard() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (!obscured()) {
    ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_SELECTION)
        .WriteText(GetSelectedText());
  }
#endif
}

bool Label::IsCommandIdChecked(int command_id) const {
  return true;
}

bool Label::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_APP_COPY:
      return HasSelection() && !obscured();
    case IDS_APP_SELECT_ALL:
      return GetRenderTextForSelectionController() && !text().empty();
  }
  return false;
}

void Label::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDS_APP_COPY:
      CopyToClipboard();
      break;
    case IDS_APP_SELECT_ALL:
      SelectAll();
      DCHECK(HasSelection());
      UpdateSelectionClipboard();
      break;
    default:
      NOTREACHED();
  }
}

bool Label::GetAcceleratorForCommandId(int command_id,
                                       ui::Accelerator* accelerator) const {
  switch (command_id) {
    case IDS_APP_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;

    case IDS_APP_SELECT_ALL:
      *accelerator = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;

    default:
      return false;
  }
}

const gfx::RenderText* Label::GetRenderTextForSelectionController() const {
  if (!selectable())
    return nullptr;
  MaybeBuildRenderTextLines();

  // This may happen when the content bounds of the view are empty.
  if (lines_.empty())
    return nullptr;

  DCHECK_EQ(1u, lines_.size());
  return lines_[0].get();
}

void Label::Init(const base::string16& text, const gfx::FontList& font_list) {
  render_text_.reset(gfx::RenderText::CreateInstance());
  render_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  render_text_->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_TEXT);
  // NOTE: |render_text_| should not be elided at all. This is used to keep some
  // properties and to compute the size of the string.
  render_text_->SetElideBehavior(gfx::NO_ELIDE);
  render_text_->SetFontList(font_list);
  render_text_->SetCursorEnabled(false);
  render_text_->SetWordWrapBehavior(gfx::TRUNCATE_LONG_WORDS);

  elide_behavior_ = gfx::ELIDE_TAIL;
  stored_selection_range_ = gfx::Range::InvalidRange();
  enabled_color_set_ = disabled_color_set_ = background_color_set_ = false;
  selection_text_color_set_ = selection_background_color_set_ = false;
  subpixel_rendering_enabled_ = true;
  auto_color_readability_ = true;
  multi_line_ = false;
  UpdateColorsFromTheme(GetNativeTheme());
  handles_tooltips_ = true;
  collapse_when_hidden_ = false;
  fixed_width_ = 0;
  max_width_ = 0;
  is_first_paint_text_ = true;
  SetText(text);

  // Only selectable labels will get requests to show the context menu, due to
  // CanProcessEventsWithinSubtree().
  BuildContextMenuContents();
  set_context_menu_controller(this);

  // This allows the BrowserView to pass the copy command from the Chrome menu
  // to the Label.
  AddAccelerator(ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
}

void Label::ResetLayout() {
  InvalidateLayout();
  PreferredSizeChanged();
  SchedulePaint();
  ClearRenderTextLines();
}

void Label::MaybeBuildRenderTextLines() const {
  if (!lines_.empty())
    return;

  gfx::Rect rect = GetContentsBounds();
  if (focus_behavior() != FocusBehavior::NEVER)
    rect.Inset(kFocusBorderPadding, kFocusBorderPadding);
  if (rect.IsEmpty())
    return;
  rect.Inset(-gfx::ShadowValue::GetMargin(shadows()));

  gfx::HorizontalAlignment alignment = horizontal_alignment();
  gfx::DirectionalityMode directionality = render_text_->directionality_mode();
  if (multi_line()) {
    // Force the directionality and alignment of the first line on other lines.
    bool rtl =
        render_text_->GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT;
    if (alignment == gfx::ALIGN_TO_HEAD)
      alignment = rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
    directionality =
        rtl ? gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR;
  }

  // Text eliding is not supported for multi-lined Labels.
  // TODO(mukai): Add multi-lined elided text support.
  gfx::ElideBehavior elide_behavior =
      multi_line() ? gfx::NO_ELIDE : elide_behavior_;
  if (!multi_line() || render_text_->MultilineSupported()) {
    std::unique_ptr<gfx::RenderText> render_text =
        CreateRenderText(text(), alignment, directionality, elide_behavior);
    render_text->SetDisplayRect(rect);
    render_text->SetMultiline(multi_line());
    render_text->SetWordWrapBehavior(render_text_->word_wrap_behavior());

    // Setup render text for selection controller.
    if (selectable()) {
      render_text->set_focused(HasFocus());
      if (stored_selection_range_.IsValid())
        render_text->SelectRange(stored_selection_range_);
    }

    lines_.push_back(std::move(render_text));
  } else {
    std::vector<base::string16> lines = GetLinesForWidth(rect.width());
    if (lines.size() > 1)
      rect.set_height(std::max(line_height(), font_list().GetHeight()));

    const int bottom = GetContentsBounds().bottom();
    for (size_t i = 0; i < lines.size() && rect.y() <= bottom; ++i) {
      std::unique_ptr<gfx::RenderText> line =
          CreateRenderText(lines[i], alignment, directionality, elide_behavior);
      line->SetDisplayRect(rect);
      lines_.push_back(std::move(line));
      rect.set_y(rect.y() + rect.height());
    }
    // Append the remaining text to the last visible line.
    for (size_t i = lines_.size(); i < lines.size(); ++i)
      lines_.back()->SetText(lines_.back()->text() + lines[i]);
  }

  stored_selection_range_ = gfx::Range::InvalidRange();
  ApplyTextColors();
}

gfx::Rect Label::GetFocusBounds() const {
  MaybeBuildRenderTextLines();

  gfx::Rect focus_bounds;
  if (lines_.empty()) {
    focus_bounds = gfx::Rect(GetTextSize());
  } else {
    for (size_t i = 0; i < lines_.size(); ++i) {
      gfx::Point origin;
      origin += lines_[i]->GetLineOffset(0);
      focus_bounds.Union(gfx::Rect(origin, lines_[i]->GetStringSize()));
    }
  }

  focus_bounds.Inset(-kFocusBorderPadding, -kFocusBorderPadding);
  focus_bounds.Intersect(GetLocalBounds());
  return focus_bounds;
}

std::vector<base::string16> Label::GetLinesForWidth(int width) const {
  std::vector<base::string16> lines;
  // |width| can be 0 when getting the default text size, in that case
  // the ideal lines (i.e. broken at newline characters) are wanted.
  if (width <= 0) {
    lines = base::SplitString(
        render_text_->GetDisplayText(), base::string16(1, '\n'),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  } else {
    gfx::ElideRectangleText(render_text_->GetDisplayText(), font_list(), width,
                            std::numeric_limits<int>::max(),
                            render_text_->word_wrap_behavior(), &lines);
  }
  return lines;
}

gfx::Size Label::GetTextSize() const {
  gfx::Size size;
  if (text().empty()) {
    size = gfx::Size(0, std::max(line_height(), font_list().GetHeight()));
  } else if (!multi_line() || render_text_->MultilineSupported()) {
    // Cancel the display rect of |render_text_|. The display rect may be
    // specified in GetHeightForWidth(), and specifying empty Rect cancels
    // its effect. See also the comment in GetHeightForWidth().
    // TODO(mukai): use gfx::Rect() to compute the ideal size rather than
    // the current width(). See crbug.com/468494, crbug.com/467526, and
    // the comment for MultilinePreferredSizeTest in label_unittest.cc.
    render_text_->SetDisplayRect(gfx::Rect(0, 0, width(), 0));
    size = render_text_->GetStringSize();
  } else {
    // Get the natural text size, unelided and only wrapped on newlines.
    std::vector<base::string16> lines = GetLinesForWidth(width());
    std::unique_ptr<gfx::RenderText> render_text(
        gfx::RenderText::CreateInstance());
    render_text->SetFontList(font_list());
    for (size_t i = 0; i < lines.size(); ++i) {
      render_text->SetText(lines[i]);
      const gfx::Size line = render_text->GetStringSize();
      size.set_width(std::max(size.width(), line.width()));
      size.set_height(std::max(line_height(), size.height() + line.height()));
    }
  }
  const gfx::Insets shadow_margin = -gfx::ShadowValue::GetMargin(shadows());
  size.Enlarge(shadow_margin.width(), shadow_margin.height());
  return size;
}

void Label::RecalculateColors() {
  actual_enabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_enabled_color_,
                                    background_color_) :
      requested_enabled_color_;
  actual_disabled_color_ = auto_color_readability_ ?
      color_utils::GetReadableColor(requested_disabled_color_,
                                    background_color_) :
      requested_disabled_color_;
  actual_selection_text_color_ =
      auto_color_readability_
          ? color_utils::GetReadableColor(requested_selection_text_color_,
                                          selection_background_color_)
          : requested_selection_text_color_;

  ApplyTextColors();
  SchedulePaint();
}

void Label::ApplyTextColors() const {
  SkColor color = enabled() ? actual_enabled_color_ : actual_disabled_color_;
  bool subpixel_rendering_suppressed =
      SkColorGetA(background_color_) != 0xFF || !subpixel_rendering_enabled_;
  for (size_t i = 0; i < lines_.size(); ++i) {
    lines_[i]->SetColor(color);
    lines_[i]->set_selection_color(actual_selection_text_color_);
    lines_[i]->set_selection_background_focused_color(
        selection_background_color_);
    lines_[i]->set_subpixel_rendering_suppressed(subpixel_rendering_suppressed);
  }
}

void Label::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if (!enabled_color_set_) {
    requested_enabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelEnabledColor);
  }
  if (!disabled_color_set_) {
    requested_disabled_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelDisabledColor);
  }
  if (!background_color_set_) {
    background_color_ =
        theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);
  }
  if (!selection_text_color_set_) {
    requested_selection_text_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelTextSelectionColor);
  }
  if (!selection_background_color_set_) {
    selection_background_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelTextSelectionBackgroundFocused);
  }
  RecalculateColors();
}

bool Label::ShouldShowDefaultTooltip() const {
  const gfx::Size text_size = GetTextSize();
  const gfx::Size size = GetContentsBounds().size();
  return !obscured() && (text_size.width() > size.width() ||
                         (multi_line() && text_size.height() > size.height()));
}

void Label::ClearRenderTextLines() const {
  // The HasSelection() call below will build |lines_| in case it is empty.
  // Return early to avoid this.
  if (lines_.empty())
    return;

  // Persist the selection range if there is an active selection.
  if (HasSelection()) {
    stored_selection_range_ =
        GetRenderTextForSelectionController()->selection();
  }
  lines_.clear();
}

base::string16 Label::GetSelectedText() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? render_text->GetTextFromRange(render_text->selection())
                     : base::string16();
}

void Label::CopyToClipboard() {
  if (!HasSelection() || obscured())
    return;
  ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_COPY_PASTE)
      .WriteText(GetSelectedText());
}

void Label::BuildContextMenuContents() {
  context_menu_contents_.AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
  context_menu_contents_.AddItemWithStringId(IDS_APP_SELECT_ALL,
                                             IDS_APP_SELECT_ALL);
}

}  // namespace views
