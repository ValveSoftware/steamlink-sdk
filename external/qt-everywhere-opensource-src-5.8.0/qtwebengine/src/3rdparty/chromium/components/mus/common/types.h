// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_TYPES_H_
#define COMPONENTS_MUS_COMMON_TYPES_H_

#include <stdint.h>

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace mus {

// Used to identify windows and change ids.
typedef uint32_t Id;

// Used to identify a client as well as a client-specific window id. For
// example, the Id for a window consists of the ClientSpecificId of the client
// and the ClientSpecificId of the window.
typedef uint16_t ClientSpecificId;

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_TYPES_H_
