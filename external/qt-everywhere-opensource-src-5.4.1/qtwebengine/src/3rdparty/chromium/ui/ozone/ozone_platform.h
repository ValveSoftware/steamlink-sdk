// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_OZONE_PLATFORM_H_
#define UI_OZONE_OZONE_PLATFORM_H_

#include "base/memory/scoped_ptr.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

class CursorFactoryOzone;
class EventFactoryOzone;
class NativeDisplayDelegate;
class SurfaceFactoryOzone;
class TouchscreenDeviceManager;
class GpuPlatformSupport;
class GpuPlatformSupportHost;

// Base class for Ozone platform implementations.
//
// Ozone platforms must override this class and implement the virtual
// GetFooFactoryOzone() methods to provide implementations of the
// various ozone interfaces.
//
// The OzonePlatform subclass can own any state needed by the
// implementation that is shared between the various ozone interfaces,
// such as a connection to the windowing system.
//
// A platform is free to use different implementations of each
// interface depending on the context. You can, for example, create
// different objects depending on the underlying hardware, command
// line flags, or whatever is appropriate for the platform.
class OZONE_EXPORT OzonePlatform {
 public:
  OzonePlatform();
  virtual ~OzonePlatform();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events, surface, etc.)
  static void InitializeForUI();

  // Initializes the subsystems/resources necessary for the GPU process.
  static void InitializeForGPU();

  static OzonePlatform* GetInstance();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() = 0;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() = 0;
  virtual ui::GpuPlatformSupport* GetGpuPlatformSupport() = 0;
  virtual ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<ui::NativeDisplayDelegate>
      CreateNativeDisplayDelegate() = 0;
  virtual scoped_ptr<ui::TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() = 0;
#endif

 private:
  virtual void InitializeUI() = 0;
  virtual void InitializeGPU() = 0;

  static void CreateInstance();

  static OzonePlatform* instance_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_OZONE_PLATFORM_H_
