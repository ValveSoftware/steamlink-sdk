// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_TYPES_H_
#define UI_AURA_MUS_MUS_TYPES_H_

#include <stdint.h>

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace aura {

// Used to identify windows and change ids.
using Id = uint32_t;

// Used to identify a client as well as a client-specific window id. For
// example, the Id for a window consists of the ClientSpecificId of the client
// and the ClientSpecificId of the window.
using ClientSpecificId = uint16_t;

constexpr Id kInvalidServerId = 0;

enum class WindowMusType {
  // The window is an embed root. That is, the client received this window by
  // way of another client calling Embed().
  // NOTE: in the client that called Embed() the window type is LOCAL.
  EMBED,

  // The window was created by requesting a top level
  // (WindowTree::NewTopLevel()).
  // NOTE: in the window manager (the one responsible for actually creating the
  // real window) the window is of type LOCAL.
  TOP_LEVEL,

  // The window is a display root for the window manager.
  DISPLAY,

  // The window was created locally.
  LOCAL,

  // Not one of the above. This means the window is visible to the client and
  // not one of the above values. Practially this means this client is the
  // window manager and the window was created by another client.
  OTHER,
};

}  // namespace ui

#endif  // UI_AURA_MUS_MUS_TYPES_H_
