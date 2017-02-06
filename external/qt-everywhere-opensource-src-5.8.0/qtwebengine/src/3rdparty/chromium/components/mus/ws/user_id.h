// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_USER_ID_H_
#define COMPONENTS_MUS_WS_USER_ID_H_

#include <string>

namespace mus {
namespace ws {

using UserId = std::string;

inline UserId InvalidUserId() {
  return std::string();
}

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_USER_ID_H_
