// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SURFACE_BINDING_H_
#define UI_VIEWS_MUS_SURFACE_BINDING_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "ui/views/mus/mus_export.h"

namespace cc {
class OutputSurface;
}

namespace mus {
class Window;
}

namespace shell {
class Connector;
}

namespace views {

// SurfaceBinding is responsible for managing the connections necessary to
// bind a Window to the surfaces service.
// Internally SurfaceBinding manages one connection (and related structures) per
// WindowTree. That is, all Windows from a particular WindowTree share the same
// connection.
class VIEWS_MUS_EXPORT SurfaceBinding {
 public:
  SurfaceBinding(shell::Connector* connector,
                 mus::Window* window,
                 mus::mojom::SurfaceType surface_type);
  ~SurfaceBinding();

  // Creates an OutputSurface that renders to the Window supplied to the
  // constructor.
  std::unique_ptr<cc::OutputSurface> CreateOutputSurface();

 private:
  class PerClientState;

  mus::Window* window_;
  const mus::mojom::SurfaceType surface_type_;
  scoped_refptr<PerClientState> state_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceBinding);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SURFACE_BINDING_H_
