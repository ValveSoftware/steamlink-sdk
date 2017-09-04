// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_SOURCE_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_SOURCE_H_

namespace content {

// Describes the source of where the input event ACK was
// generated from inside the renderer.
enum class InputEventAckSource {
  UNKNOWN,
  COMPOSITOR_THREAD,
  MAIN_THREAD,
  MAX = MAIN_THREAD
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_SOURCE_H_
