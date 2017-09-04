// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_TEST_UTILS_H_
#define UI_VIEWS_MUS_TEST_UTILS_H_

#include "base/memory/ptr_util.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace test {

class WindowManagerConnectionTestApi {
 public:
  explicit WindowManagerConnectionTestApi(WindowManagerConnection* connection)
      : connection_(connection) {}
  ~WindowManagerConnectionTestApi() {}

  ui::Window* GetUiWindowAtScreenPoint(const gfx::Point& point) {
    return connection_->GetUiWindowAtScreenPoint(point);
  }

  ScreenMus* screen() { return connection_->screen_.get(); }

 private:
  WindowManagerConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTestApi);
};

class MusClientTestApi {
 public:
  static std::unique_ptr<MusClient> Create(
      service_manager::Connector* connector,
      const service_manager::Identity& identity) {
    return base::WrapUnique(new MusClient(connector, identity));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MusClientTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_MUS_TEST_UTILS_H_
