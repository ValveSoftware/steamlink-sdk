// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <libdrm/i915_drm.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/test/gl_image_test_template.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace {

// These values are picked so that RGB -> YVU on the CPU converted
// back to RGB on the GPU produces the original RGB values without
// any error.
const uint8_t kImageColor[] = {0x10, 0x20, 0, 0xFF};

// Round |num| up to 64.
int RoundTo64(int num) {
  return (num + 63) & ~63;
}

// Create a YVU420 NativePixmap using the drm interface directly.
scoped_refptr<ui::NativePixmap> CreateYVU420Pixmap(const gfx::Size& size,
                                                   const uint8_t color[4]) {
  DCHECK_EQ(0, size.width() % 2);
  DCHECK_EQ(0, size.height() % 2);

  // TODO(dcastagna): move the creation of the drmbuf to minigmb, where it's
  // supposed to be, so we can abastract it and use SurfaceFactoryOzone.
  base::ScopedFD drm_fd(
      HANDLE_EINTR(open("/dev/dri/card0", O_RDWR | O_CLOEXEC)));
  DCHECK(drm_fd.is_valid()) << "Couldn't open '/dev/dri/card0'";

  std::vector<int> pitches{RoundTo64(size.width()), RoundTo64(size.width() / 2),
                           RoundTo64(size.width() / 2)};
  std::vector<int> offsets{
      0, pitches[0] * size.height() + pitches[1] * size.height() / 2,
      pitches[0] * size.height(),
  };
  size_t byte_number = pitches[0] * size.height() +
                       pitches[1] * size.height() / 2 +
                       pitches[2] * size.height() / 2;

  struct drm_i915_gem_create gem_create {};
  gem_create.size = byte_number;
  int ret = drmIoctl(drm_fd.get(), DRM_IOCTL_I915_GEM_CREATE, &gem_create);
  DCHECK(!ret) << "Can't create the GEM buffer.";
  struct drm_i915_gem_set_tiling gem_set_tiling {};
  do {
    gem_set_tiling.handle = gem_create.handle;
    gem_set_tiling.tiling_mode = I915_TILING_NONE;
    ret =
        drmIoctl(drm_fd.get(), DRM_IOCTL_I915_GEM_SET_TILING, &gem_set_tiling);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
  DCHECK_NE(-1, ret);
  int fd = -1;
  drmPrimeHandleToFD(drm_fd.get(), gem_create.handle, DRM_CLOEXEC, &fd);
  EXPECT_NE(-1, fd);

  std::vector<uint8_t> data(byte_number);
  for (int plane = 0; plane < 3; ++plane) {
    GLImageTestSupport::SetBufferDataToColor(
        size.width(), size.height(), pitches[plane], plane,
        gfx::BufferFormat::YVU_420, kImageColor, &data[offsets[plane]]);
  }

  // TODO(dcastagna): Add support for mapping planar formats with
  // ClientNativePixmap and remove this code.
  struct drm_i915_gem_pwrite gem_pwrite {};
  gem_pwrite.handle = gem_create.handle;
  gem_pwrite.size = byte_number;
  gem_pwrite.data_ptr = reinterpret_cast<uint64_t>(&data[0]);
  ret = drmIoctl(drm_fd.get(), DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite);
  DCHECK(!ret) << "Problems with drmIoctl when writing data.";

  gfx::NativePixmapHandle pixmap_handle;
  pixmap_handle.fds.emplace_back(fd, false);
  for (int i = 0; i < 3; i++) {
    pixmap_handle.strides_and_offsets.emplace_back(pitches[i], offsets[i]);
  }
  ui::SurfaceFactoryOzone* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  return surface_factory->CreateNativePixmapFromHandle(
      gfx::kNullAcceleratedWidget, size, gfx::BufferFormat::YVU_420,
      pixmap_handle);
}

// Delegate to test EGL images created directly using drm.
// YVU420 buffers can't be created yet via Ozone, we still
// want to test them since they can be imported and displayed
// via exo.
// TODO(dcastagna): once it's possible to allocate and map YV12 format
// using Ozone merge this test with GLImageOzoneNativePixmapTestDelegate.
class GLImageOzoneNativePixmapDrmTestDelegate {
 public:
  GLImageOzoneNativePixmapDrmTestDelegate() {}
  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    scoped_refptr<ui::NativePixmap> pixmap = CreateYVU420Pixmap(size, color);

    scoped_refptr<GLImageOzoneNativePixmap> image(
        new GLImageOzoneNativePixmap(size, GL_RGB_YCRCB_420_CHROMIUM));
    EXPECT_TRUE(image->Initialize(pixmap.get(), pixmap->GetBufferFormat()));
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_EXTERNAL_OES; }

  const uint8_t* GetImageColor() { return kImageColor; }

 private:
};


// TODO(crbug.com/618516) - The tests in this file can only be run
// on a real device, and not on linux desktop builds, so they are
// disabled until they can correctly detect the environment and do
INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageOzoneNativePixmapDrm,
                              GLImageTest,
                              GLImageOzoneNativePixmapDrmTestDelegate);

INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageOzoneNativePixmapDrm,
                              GLImageBindTest,
                              GLImageOzoneNativePixmapDrmTestDelegate);

}  // namespace
}  // namespace gl
