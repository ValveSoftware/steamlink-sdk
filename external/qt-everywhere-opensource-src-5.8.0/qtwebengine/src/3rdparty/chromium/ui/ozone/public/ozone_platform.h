// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
#define UI_OZONE_PUBLIC_OZONE_PLATFORM_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/ozone_export.h"

namespace gfx {
class Rect;
}

namespace shell {
class Connector;
class Connection;
}

namespace ui {

class CursorFactoryOzone;
class InputController;
class GpuPlatformSupport;
class GpuPlatformSupportHost;
class NativeDisplayDelegate;
class OverlayManagerOzone;
class PlatformWindow;
class PlatformWindowDelegate;
class SurfaceFactoryOzone;
class SystemInputInjector;

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

  // Additional initalization params for the platform. Platforms must not retain
  // a reference to this structure.
  struct InitParams {
    // Ozone may retain this pointer for later use. An Ozone platform embedder
    // must set this parameter in order for the Ozone platform implementation to
    // be able to use Mojo.
    shell::Connector* connector = nullptr;

    // Setting this to true indicates that the platform implementation should
    // operate as a single process for platforms (i.e. drm) that are usually
    // split between a main and gpu specific portion.
    bool single_process = false;
  };

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events, etc.)
  // TODO(rjkroege): Remove deprecated entry point (http://crbug.com/620934)
  static void InitializeForUI();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events) with additional properties to customize the ozone platform
  // implementation. Ozone will not retain InitParams after returning from
  // InitalizeForUI.
  static void InitializeForUI(const InitParams& args);

  // Initializes the subsystems/resources necessary for rendering (i.e. GPU).
  // TODO(rjkroege): Remove deprecated entry point (http://crbug.com/620934)
  static void InitializeForGPU();

  // Initializes the subsystems for rendering but with additional properties
  // provided by |args| as with InitalizeForUI.
  static void InitializeForGPU(const InitParams& args);

  static OzonePlatform* GetInstance();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::OverlayManagerOzone* GetOverlayManager() = 0;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() = 0;
  virtual ui::InputController* GetInputController() = 0;
  virtual ui::GpuPlatformSupport* GetGpuPlatformSupport() = 0;
  virtual ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
  virtual std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() = 0;
  virtual std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) = 0;
  virtual std::unique_ptr<ui::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() = 0;

  // Ozone platform implementations may also choose to expose mojo interfaces to
  // internal functionality. Embedders wishing to take advantage of ozone mojo
  // implementations must invoke AddInterfaces with a valid shell::Connection*
  // pointer to export all Mojo interfaces defined within Ozone.
  //
  // A default do-nothing implementation is provided to permit platform
  // implementations to opt out of implementing any Mojo interfaces.
  virtual void AddInterfaces(shell::Connection* connection);

 private:
  virtual void InitializeUI() = 0;
  virtual void InitializeGPU() = 0;
  virtual void InitializeUI(const InitParams& args);
  virtual void InitializeGPU(const InitParams& args);

  static void CreateInstance();

  static OzonePlatform* instance_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
