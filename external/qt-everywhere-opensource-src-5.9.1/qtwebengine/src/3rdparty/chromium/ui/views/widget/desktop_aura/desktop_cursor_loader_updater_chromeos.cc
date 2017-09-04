// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace views {
namespace {

// Mus handles cursor, so this class need not do anything.
class DesktopCursorLoaderUpdaterChromeos : public DesktopCursorLoaderUpdater {
 public:
  DesktopCursorLoaderUpdaterChromeos() {}
  ~DesktopCursorLoaderUpdaterChromeos() override {}

  // DesktopCursorLoaderUpdater:
  void OnCreate(float device_scale_factor, ui::CursorLoader* loader) override {}
  void OnDisplayUpdated(const display::Display& display,
                        ui::CursorLoader* loader) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopCursorLoaderUpdaterChromeos);
};

}  // namespace

// static
std::unique_ptr<DesktopCursorLoaderUpdater>
DesktopCursorLoaderUpdater::Create() {
  std::unique_ptr<DesktopCursorLoaderUpdaterChromeos> loader =
      base::MakeUnique<DesktopCursorLoaderUpdaterChromeos>();
  return std::move(loader);
}

}  // namespace views
