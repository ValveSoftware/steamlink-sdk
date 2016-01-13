// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
#define MOJO_AURA_CONTEXT_FACTORY_MOJO_H_

#include "ui/compositor/compositor.h"

namespace mojo {

class ContextFactoryMojo : public ui::ContextFactory {
 public:
  ContextFactoryMojo();
  virtual ~ContextFactoryMojo();

 private:
  // ContextFactory:
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Compositor* compositor,
      bool software_fallback) OVERRIDE;
  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) OVERRIDE;
  virtual void RemoveReflector(scoped_refptr<ui::Reflector> reflector) OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider()
      OVERRIDE;
  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE;
  virtual bool DoesCreateTestContexts() OVERRIDE;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() OVERRIDE;
  virtual base::MessageLoopProxy* GetCompositorMessageLoop() OVERRIDE;

  scoped_ptr<cc::SharedBitmapManager> shared_bitmap_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContextFactoryMojo);
};

}  // namespace mojo

#endif  // MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
