// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
namespace examples {

ComboboxModelExample::ComboboxModelExample() {
}

ComboboxModelExample::~ComboboxModelExample() {
}

int ComboboxModelExample::GetItemCount() const {
  return 10;
}

base::string16 ComboboxModelExample::GetItemAt(int index) {
  return base::UTF8ToUTF16(base::StringPrintf("Item %d", index));
}

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box"), combobox_(NULL) {
}

ComboboxExample::~ComboboxExample() {
  // Delete |combobox_| first as it references |combobox_model_|.
  delete combobox_;
  combobox_ = NULL;
}

void ComboboxExample::CreateExampleView(View* container) {
  combobox_ = new Combobox(&combobox_model_);
  combobox_->set_listener(this);
  combobox_->SetSelectedIndex(3);

  container->SetLayoutManager(new FillLayout);
  container->AddChildView(combobox_);
}

void ComboboxExample::OnPerformAction(Combobox* combobox) {
  DCHECK_EQ(combobox_, combobox);
  PrintStatus("Selected: %s", base::UTF16ToUTF8(combobox_model_.GetItemAt(
      combobox->selected_index())).c_str());
}

}  // namespace examples
}  // namespace views
