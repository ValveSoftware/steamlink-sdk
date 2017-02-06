// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_

namespace mus {

class WindowSurface;

class WindowSurfaceClient {
 public:
  virtual void OnResourcesReturned(
      WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) = 0;

 protected:
  virtual ~WindowSurfaceClient() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_
