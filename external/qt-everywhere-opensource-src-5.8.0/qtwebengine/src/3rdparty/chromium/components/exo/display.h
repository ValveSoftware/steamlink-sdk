// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DISPLAY_H_
#define COMPONENTS_EXO_DISPLAY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/shared_memory_handle.h"

#if defined(USE_OZONE)
#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#endif

namespace gfx {
class Point;
}

namespace exo {
class NotificationSurface;
class NotificationSurfaceManager;
class SharedMemory;
class ShellSurface;
class SubSurface;
class Surface;

#if defined(USE_OZONE)
class Buffer;
#endif

// The core display class. This class provides functions for creating surfaces
// and is in charge of combining the contents of multiple surfaces into one
// displayable output.
class Display {
 public:
  Display();
  explicit Display(NotificationSurfaceManager* notification_surface_manager);
  ~Display();

  // Creates a new surface.
  std::unique_ptr<Surface> CreateSurface();

  // Creates a shared memory segment from |handle| of |size| with the
  // given |id|. This function takes ownership of |handle|.
  std::unique_ptr<SharedMemory> CreateSharedMemory(
      const base::SharedMemoryHandle& handle,
      size_t size);

#if defined(USE_OZONE)
  // Creates a buffer for a Linux DMA-buf file descriptor.
  std::unique_ptr<Buffer> CreateLinuxDMABufBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      const std::vector<int>& strides,
      const std::vector<int>& offsets,
      std::vector<base::ScopedFD>&& fds);
#endif

  // Creates a shell surface for an existing surface.
  std::unique_ptr<ShellSurface> CreateShellSurface(Surface* surface);

  // Creates a popup shell surface for an existing surface at |position| and
  // with |parent|. |position| is in |parent| surface local coordinates.
  std::unique_ptr<ShellSurface> CreatePopupShellSurface(
      Surface* surface,
      ShellSurface* parent,
      const gfx::Point& position);

  // Creates a remote shell surface for an existing surface using |container|.
  std::unique_ptr<ShellSurface> CreateRemoteShellSurface(Surface* surface,
                                                         int container);

  // Creates a sub-surface for an existing surface. The sub-surface will be
  // a child of |parent|.
  std::unique_ptr<SubSurface> CreateSubSurface(Surface* surface,
                                               Surface* parent);

  // Creates a notification surface for a surface and notification id.
  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_id);

 private:
  NotificationSurfaceManager* const notification_surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DISPLAY_H_
