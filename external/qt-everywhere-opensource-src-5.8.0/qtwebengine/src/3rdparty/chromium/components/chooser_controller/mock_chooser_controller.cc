// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chooser_controller/mock_chooser_controller.h"

MockChooserController::MockChooserController(content::RenderFrameHost* owner)
    : ChooserController(owner) {}

MockChooserController::~MockChooserController() {}

size_t MockChooserController::NumOptions() const {
  return option_names_.size();
}

const base::string16& MockChooserController::GetOption(size_t index) const {
  return option_names_[index];
}

void MockChooserController::OptionAdded(const base::string16 option_name) {
  option_names_.push_back(option_name);
  if (observer())
    observer()->OnOptionAdded(option_names_.size() - 1);
}

void MockChooserController::OptionRemoved(const base::string16 option_name) {
  for (auto it = option_names_.begin(); it != option_names_.end(); ++it) {
    if (*it == option_name) {
      size_t index = it - option_names_.begin();
      option_names_.erase(it);
      if (observer())
        observer()->OnOptionRemoved(index);
      return;
    }
  }
}
