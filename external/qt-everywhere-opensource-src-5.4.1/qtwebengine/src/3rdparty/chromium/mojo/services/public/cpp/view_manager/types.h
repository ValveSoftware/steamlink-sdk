// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_TYPES_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_TYPES_H_

#include "base/basictypes.h"

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace mojo {
namespace view_manager {

// Used to identify nodes, views and change ids.
typedef uint32_t Id;

// Used to identify a connection as well as a connection specific view or node
// id. For example, the Id for a node consists of the ConnectionSpecificId of
// the connection and the ConnectionSpecificId of the node.
typedef uint16_t ConnectionSpecificId;

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_TYPES_H_
