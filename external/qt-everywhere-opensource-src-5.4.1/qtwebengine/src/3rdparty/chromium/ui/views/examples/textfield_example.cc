// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textfield_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;

namespace views {
namespace examples {

TextfieldExample::TextfieldExample()
   : ExampleBase("Textfield"),
     name_(NULL),
     password_(NULL),
     read_only_(NULL),
     show_password_(NULL),
     clear_all_(NULL),
     append_(NULL),
     set_(NULL),
     set_style_(NULL) {
}

TextfieldExample::~TextfieldExample() {
}

void TextfieldExample::CreateExampleView(View* container) {
  name_ = new Textfield();
  password_ = new Textfield();
  password_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  password_->set_placeholder_text(ASCIIToUTF16("password"));
  read_only_ = new Textfield();
  read_only_->SetReadOnly(true);
  read_only_->SetText(ASCIIToUTF16("read only"));
  show_password_ = new LabelButton(this, ASCIIToUTF16("Show password"));
  clear_all_ = new LabelButton(this, ASCIIToUTF16("Clear All"));
  append_ = new LabelButton(this, ASCIIToUTF16("Append"));
  set_ = new LabelButton(this, ASCIIToUTF16("Set"));
  set_style_ = new LabelButton(this, ASCIIToUTF16("Set Styles"));
  name_->set_controller(this);
  password_->set_controller(this);

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0.2f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        0.8f, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Name:")));
  layout->AddView(name_);
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Password:")));
  layout->AddView(password_);
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Read Only:")));
  layout->AddView(read_only_);
  layout->StartRow(0, 0);
  layout->AddView(show_password_);
  layout->StartRow(0, 0);
  layout->AddView(clear_all_);
  layout->StartRow(0, 0);
  layout->AddView(append_);
  layout->StartRow(0, 0);
  layout->AddView(set_);
  layout->StartRow(0, 0);
  layout->AddView(set_style_);
}

void TextfieldExample::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  if (sender == name_) {
    PrintStatus("Name [%s]", UTF16ToUTF8(new_contents).c_str());
  } else if (sender == password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(new_contents).c_str());
  } else if (sender == read_only_) {
    PrintStatus("Read Only [%s]", UTF16ToUTF8(new_contents).c_str());
  }
}

bool TextfieldExample::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  return false;
}

bool TextfieldExample::HandleMouseEvent(Textfield* sender,
                                        const ui::MouseEvent& mouse_event) {
  PrintStatus("HandleMouseEvent click count=%d", mouse_event.GetClickCount());
  return false;
}

void TextfieldExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == show_password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(password_->text()).c_str());
  } else if (sender == clear_all_) {
    base::string16 empty;
    name_->SetText(empty);
    password_->SetText(empty);
    read_only_->SetText(empty);
  } else if (sender == append_) {
    name_->AppendText(ASCIIToUTF16("[append]"));
    password_->AppendText(ASCIIToUTF16("[append]"));
    read_only_->AppendText(ASCIIToUTF16("[append]"));
  } else if (sender == set_) {
    name_->SetText(ASCIIToUTF16("[set]"));
    password_->SetText(ASCIIToUTF16("[set]"));
    read_only_->SetText(ASCIIToUTF16("[set]"));
  } else if (sender == set_style_) {
    if (!name_->text().empty()) {
      name_->SetColor(SK_ColorGREEN);
      name_->SetStyle(gfx::BOLD, true);

      if (name_->text().length() >= 5) {
        size_t fifth = name_->text().length() / 5;
        const gfx::Range big_range(1 * fifth, 4 * fifth);
        name_->ApplyStyle(gfx::BOLD, false, big_range);
        name_->ApplyStyle(gfx::UNDERLINE, true, big_range);
        name_->ApplyColor(SK_ColorBLUE, big_range);

        const gfx::Range small_range(2 * fifth, 3 * fifth);
        name_->ApplyStyle(gfx::ITALIC, true, small_range);
        name_->ApplyStyle(gfx::UNDERLINE, false, small_range);
        name_->ApplyStyle(gfx::DIAGONAL_STRIKE, true, small_range);
        name_->ApplyColor(SK_ColorRED, small_range);
      }
    }
  }
}

}  // namespace examples
}  // namespace views
