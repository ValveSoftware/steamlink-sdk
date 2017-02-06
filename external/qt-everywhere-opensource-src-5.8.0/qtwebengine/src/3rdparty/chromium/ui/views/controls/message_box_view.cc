// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/message_box_view.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

const int kDefaultMessageWidth = 400;

// Paragraph separators are defined in
// http://www.unicode.org/Public/6.0.0/ucd/extracted/DerivedBidiClass.txt
//
// # Bidi_Class=Paragraph_Separator
//
// 000A          ; B # Cc       <control-000A>
// 000D          ; B # Cc       <control-000D>
// 001C..001E    ; B # Cc   [3] <control-001C>..<control-001E>
// 0085          ; B # Cc       <control-0085>
// 2029          ; B # Zp       PARAGRAPH SEPARATOR
bool IsParagraphSeparator(base::char16 c) {
  return ( c == 0x000A || c == 0x000D || c == 0x001C || c == 0x001D ||
           c == 0x001E || c == 0x0085 || c == 0x2029);
}

// Splits |text| into a vector of paragraphs.
// Given an example "\nabc\ndef\n\n\nhij\n", the split results should be:
// "", "abc", "def", "", "", "hij", and "".
void SplitStringIntoParagraphs(const base::string16& text,
                               std::vector<base::string16>* paragraphs) {
  paragraphs->clear();

  size_t start = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (IsParagraphSeparator(text[i])) {
      paragraphs->push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  paragraphs->push_back(text.substr(start, text.length() - start));
}

}  // namespace

namespace views {

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, public:

// static
const char MessageBoxView::kViewClassName[] = "MessageBoxView";

MessageBoxView::InitParams::InitParams(const base::string16& message)
    : options(NO_OPTIONS),
      message(message),
      message_width(kDefaultMessageWidth),
      inter_row_vertical_spacing(kRelatedControlVerticalSpacing) {}

MessageBoxView::InitParams::~InitParams() {
}

MessageBoxView::MessageBoxView(const InitParams& params)
    : prompt_field_(NULL),
      checkbox_(NULL),
      link_(NULL),
      message_width_(params.message_width) {
  Init(params);
}

MessageBoxView::~MessageBoxView() {}

base::string16 MessageBoxView::GetInputText() {
  return prompt_field_ ? prompt_field_->text() : base::string16();
}

bool MessageBoxView::IsCheckBoxSelected() {
  return checkbox_ ? checkbox_->checked() : false;
}

void MessageBoxView::SetCheckBoxLabel(const base::string16& label) {
  if (!checkbox_)
    checkbox_ = new Checkbox(label);
  else
    checkbox_->SetText(label);
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxSelected(bool selected) {
  if (!checkbox_)
    return;
  checkbox_->SetChecked(selected);
}

void MessageBoxView::SetLink(const base::string16& text,
                             LinkListener* listener) {
  if (text.empty()) {
    DCHECK(!listener);
    delete link_;
    link_ = NULL;
  } else {
    DCHECK(listener);
    if (!link_) {
      link_ = new Link();
      link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
    link_->SetText(text);
    link_->set_listener(listener);
  }
  ResetLayoutManager();
}

void MessageBoxView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_ALERT;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, View overrides:

void MessageBoxView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child == this && details.is_add) {
    if (prompt_field_)
      prompt_field_->SelectAll(true);

    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  }
}

bool MessageBoxView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // We only accepts Ctrl-C.
  DCHECK(accelerator.key_code() == 'C' && accelerator.IsCtrlDown());

  // We must not intercept Ctrl-C when we have a text box and it's focused.
  if (prompt_field_ && prompt_field_->HasFocus())
    return false;

  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  base::string16 text = message_labels_[0]->text();
  for (size_t i = 1; i < message_labels_.size(); ++i)
    text += message_labels_[i]->text();
  scw.WriteText(text);
  return true;
}

const char* MessageBoxView::GetClassName() const {
  return kViewClassName;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::Init(const InitParams& params) {
  if (params.options & DETECT_DIRECTIONALITY) {
    std::vector<base::string16> texts;
    SplitStringIntoParagraphs(params.message, &texts);
    for (size_t i = 0; i < texts.size(); ++i) {
      Label* message_label = new Label(texts[i]);
      // Avoid empty multi-line labels, which have a height of 0.
      message_label->SetMultiLine(!texts[i].empty());
      message_label->SetAllowCharacterBreak(true);
      message_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
      message_labels_.push_back(message_label);
    }
  } else {
    Label* message_label = new Label(params.message);
    message_label->SetMultiLine(true);
    message_label->SetAllowCharacterBreak(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_labels_.push_back(message_label);
  }

  if (params.options & HAS_PROMPT_FIELD) {
    prompt_field_ = new Textfield;
    prompt_field_->SetText(params.default_prompt);
  }

  inter_row_vertical_spacing_ = params.inter_row_vertical_spacing;

  ResetLayoutManager();
}

void MessageBoxView::ResetLayoutManager() {
  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Add the column set for the message displayed at the top of the dialog box.
  const int message_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, message_width_, 0);

  // Column set for extra elements, if any.
  const int extra_column_view_set_id = 1;
  if (prompt_field_ || checkbox_ || link_) {
    column_set = layout->AddColumnSet(extra_column_view_set_id);
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  const int kMaxScrollViewHeight = 400;
  views::View* message_contents = new views::View();
  message_contents->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  for (size_t i = 0; i < message_labels_.size(); ++i)
    message_contents->AddChildView(message_labels_[i]);
  ScrollView* scroll_view = new views::ScrollView();
  scroll_view->ClipHeightTo(0, kMaxScrollViewHeight);
  scroll_view->SetContents(message_contents);
  layout->StartRow(0, message_column_view_set_id);
  layout->AddView(scroll_view);

  if (prompt_field_) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, extra_column_view_set_id);
    layout->AddView(prompt_field_);
  }

  if (checkbox_) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, extra_column_view_set_id);
    layout->AddView(checkbox_);
  }

  if (link_) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, extra_column_view_set_id);
    layout->AddView(link_);
  }
}

}  // namespace views
