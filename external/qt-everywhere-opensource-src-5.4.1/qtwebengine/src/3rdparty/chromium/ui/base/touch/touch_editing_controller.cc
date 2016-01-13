// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_editing_controller.h"

namespace ui {

namespace {
TouchSelectionControllerFactory* g_shared_instance = NULL;
}  // namespace

TouchSelectionController* TouchSelectionController::create(
    TouchEditable* client_view) {
  if (g_shared_instance)
    return g_shared_instance->create(client_view);
  return NULL;
}

// static
void TouchSelectionControllerFactory::SetInstance(
    TouchSelectionControllerFactory* instance) {
  g_shared_instance = instance;
}

}  // namespace ui
