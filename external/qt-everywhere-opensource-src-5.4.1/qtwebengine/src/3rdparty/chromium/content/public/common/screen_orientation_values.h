// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SCREEN_ORIENTATION_VALUES_H_
#define CONTENT_PUBLIC_COMMON_SCREEN_ORIENTATION_VALUES_H_

namespace content {

enum ScreenOrientationValues {
#define DEFINE_SCREEN_ORIENTATION_VALUE(name, value) name = value,
#include "content/public/common/screen_orientation_values_list.h"
#undef DEFINE_SCREEN_ORIENTATION_VALUE
};

} // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SCREEN_ORIENTATION_VALUES_H_
