// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONSOLE_MESSAGE_LEVEL_H_
#define CONTENT_PUBLIC_COMMON_CONSOLE_MESSAGE_LEVEL_H_

namespace content {

enum ConsoleMessageLevel {
  CONSOLE_MESSAGE_LEVEL_DEBUG,
  CONSOLE_MESSAGE_LEVEL_LOG,
  CONSOLE_MESSAGE_LEVEL_WARNING,
  CONSOLE_MESSAGE_LEVEL_ERROR,
  CONSOLE_MESSAGE_LEVEL_LAST = CONSOLE_MESSAGE_LEVEL_ERROR
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONSOLE_MESSAGE_LEVEL_H_
