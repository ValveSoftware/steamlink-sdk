// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/toolbar_model.h"

ToolbarModel::ToolbarModel()
    : input_in_progress_(false),
      url_replacement_enabled_(true) {
}

ToolbarModel::~ToolbarModel() {
}

bool ToolbarModel::WouldReplaceURL() const {
  return WouldPerformSearchTermReplacement(false);
}

