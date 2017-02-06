// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_

#include <memory>

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace shell {
class Connector;
}

namespace mus {

class WindowManagerDelegate;
class WindowTreeClientDelegate;

// The following create a new window tree host. Supply a |factory| if you have
// already connected to mus, otherwise supply |shell|, which contacts mus and
// obtains a WindowTreeHostFactory.
void CreateWindowTreeHost(mojom::WindowTreeHostFactory* factory,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate);
void CreateWindowTreeHost(shell::Connector* connector,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate);

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
