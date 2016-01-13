// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_FILE_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_TEST_FILE_SURFACE_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class OZONE_BASE_EXPORT FileSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit FileSurfaceFactory(const base::FilePath& dump_location);
  virtual ~FileSurfaceFactory();

 private:
  // SurfaceFactoryOzone:
  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  base::FilePath location_;

  DISALLOW_COPY_AND_ASSIGN(FileSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_FILE_SURFACE_FACTORY_H_
