// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_

#include <cstddef>

#include "build/build_config.h"

namespace content {

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
class ZygoteCommunication;
using ZygoteHandle = ZygoteCommunication*;
#else
using ZygoteHandle = std::nullptr_t;
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_
