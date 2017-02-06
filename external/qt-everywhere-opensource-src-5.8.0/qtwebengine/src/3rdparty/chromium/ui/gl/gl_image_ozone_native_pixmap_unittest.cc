// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/test/gl_image_test_template.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace {

const uint8_t kRed[] = {0xF0, 0x0, 0x0, 0xFF};
const uint8_t kGreen[] = {0x0, 0xFF, 0x0, 0xFF};

template <gfx::BufferUsage usage, gfx::BufferFormat format>
class GLImageOzoneNativePixmapTestDelegate {
 public:
  GLImageOzoneNativePixmapTestDelegate() {
    client_pixmap_factory_ = ui::ClientNativePixmapFactory::Create();
  }
  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<ui::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                            format, usage);
    DCHECK(pixmap);
    if (usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE) {
      auto client_pixmap = client_pixmap_factory_->ImportFromHandle(
          pixmap->ExportHandle(), size, usage);
      void* data = client_pixmap->Map();
      EXPECT_TRUE(data);
      GLImageTestSupport::SetBufferDataToColor(
          size.width(), size.height(), pixmap->GetDmaBufPitch(0), 0,
          pixmap->GetBufferFormat(), color, static_cast<uint8_t*>(data));
      client_pixmap->Unmap();
    }

    scoped_refptr<GLImageOzoneNativePixmap> image(new GLImageOzoneNativePixmap(
        size, GLImageOzoneNativePixmap::GetInternalFormatForTesting(format)));
    EXPECT_TRUE(image->Initialize(pixmap.get(), pixmap->GetBufferFormat()));
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_EXTERNAL_OES; }

  const uint8_t* GetImageColor() {
    return format == gfx::BufferFormat::R_8 ? kRed : kGreen;
  }

 private:
  std::unique_ptr<ui::ClientNativePixmapFactory> client_pixmap_factory_;
};

using GLImageScanoutType = testing::Types<
    GLImageOzoneNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                         gfx::BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_CASE_P(GLImageOzoneNativePixmapScanout,
                              GLImageTest,
                              GLImageScanoutType);

using GLImageReadWriteType =
    testing::Types<GLImageOzoneNativePixmapTestDelegate<
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferFormat::R_8>>;

using GLImageBindTestTypes =
    testing::Types<GLImageOzoneNativePixmapTestDelegate<
                       gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                       gfx::BufferFormat::BGRA_8888>,
                   GLImageOzoneNativePixmapTestDelegate<
                       gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                       gfx::BufferFormat::R_8>>;

// These tests are disabled since the trybots are running with Ozone X11
// implementation that doesn't support creating ClientNativePixmap.
// TODO(dcastagna): Implement ClientNativePixmapFactory on Ozone X11.
INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageOzoneNativePixmapReadWrite,
                              GLImageTest,
                              GLImageReadWriteType);

INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageOzoneNativePixmap,
                              GLImageBindTest,
                              GLImageBindTestTypes);

}  // namespace
}  // namespace gl
