// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_USER_ID_H_
#define SERVICES_UI_WS_USER_ID_H_

#include <string>

namespace ui {
namespace ws {

using UserId = std::string;

inline UserId InvalidUserId() {
  return std::string();
}

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_USER_ID_H_
