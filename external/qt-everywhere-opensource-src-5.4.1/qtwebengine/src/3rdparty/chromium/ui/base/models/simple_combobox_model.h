// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_

#include "ui/base/models/combobox_model.h"

#include <vector>

namespace ui {

// A simple data model for a combobox that takes a string16 vector as the items.
// An empty string will be a separator.
class UI_BASE_EXPORT SimpleComboboxModel : public ComboboxModel {
 public:
  explicit SimpleComboboxModel(const std::vector<base::string16>& items);
  virtual ~SimpleComboboxModel();

  // ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual bool IsItemSeparatorAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;

 private:
  const std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SimpleComboboxModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
