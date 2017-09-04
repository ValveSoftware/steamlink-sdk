// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/gl/gl_image_ozone_native_pixmap.h"

#include <vector>

#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_surface_egl.h"

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_YV12 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')

namespace ui {
namespace {

bool ValidInternalFormat(unsigned internalformat, gfx::BufferFormat format) {
  switch (internalformat) {
    case GL_RGB:
      return format == gfx::BufferFormat::BGR_565 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::BGRX_8888;
    case GL_RGB_YCRCB_420_CHROMIUM:
      return format == gfx::BufferFormat::YVU_420;
    case GL_RGB_YCBCR_420V_CHROMIUM:
      return format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case GL_RGBA:
      return format == gfx::BufferFormat::RGBA_8888;
    case GL_BGRA_EXT:
      return format == gfx::BufferFormat::BGRA_8888;
    case GL_RED_EXT:
      return format == gfx::BufferFormat::R_8;
    case GL_RG_EXT:
      return format == gfx::BufferFormat::RG_88;
    default:
      return false;
  }
}

bool ValidFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return true;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

EGLint FourCC(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::RG_88:
      return DRM_FORMAT_GR88;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YV12;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

GLImageOzoneNativePixmap::GLImageOzoneNativePixmap(const gfx::Size& size,
                                                   unsigned internalformat)
    : GLImageEGL(size),
      internalformat_(internalformat),
      has_image_flush_external_(
          gl::GLSurfaceEGL::HasEGLExtension("EGL_EXT_image_flush_external")) {}

GLImageOzoneNativePixmap::~GLImageOzoneNativePixmap() {}

bool GLImageOzoneNativePixmap::Initialize(NativePixmap* pixmap,
                                          gfx::BufferFormat format) {
  DCHECK(!pixmap_);
  if (pixmap->GetEGLClientBuffer()) {
    EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    if (!GLImageEGL::Initialize(EGL_NATIVE_PIXMAP_KHR,
                                pixmap->GetEGLClientBuffer(), attrs)) {
      return false;
    }
  } else if (pixmap->AreDmaBufFdsValid()) {
    if (!ValidFormat(format)) {
      LOG(ERROR) << "Invalid format: " << static_cast<int>(format);
      return false;
    }

    if (!ValidInternalFormat(internalformat_, format)) {
      LOG(ERROR) << "Invalid internalformat: " << internalformat_
                 << " for format: " << static_cast<int>(format);
      return false;
    }

    // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
    // target, the EGL will take a reference to the dma_buf.
    std::vector<EGLint> attrs;
    attrs.push_back(EGL_WIDTH);
    attrs.push_back(size_.width());
    attrs.push_back(EGL_HEIGHT);
    attrs.push_back(size_.height());
    attrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
    attrs.push_back(FourCC(format));

    const EGLint kLinuxDrmModifiers[] = {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                                         EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                         EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT};
    bool has_dma_buf_import_modifier = gl::GLSurfaceEGL::HasEGLExtension(
        "EGL_EXT_image_dma_buf_import_modifiers");

    for (size_t attrs_plane = 0;
         attrs_plane <
         gfx::NumberOfPlanesForBufferFormat(pixmap->GetBufferFormat());
         ++attrs_plane) {
      attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT + attrs_plane * 3);

      size_t pixmap_plane = attrs_plane;

      attrs.push_back(pixmap->GetDmaBufFd(
          pixmap_plane < pixmap->GetDmaBufFdCount() ? pixmap_plane : 0));
      attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT + attrs_plane * 3);
      attrs.push_back(pixmap->GetDmaBufOffset(pixmap_plane));
      attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT + attrs_plane * 3);
      attrs.push_back(pixmap->GetDmaBufPitch(pixmap_plane));
      if (has_dma_buf_import_modifier) {
        uint64_t modifier = pixmap->GetDmaBufModifier(pixmap_plane);
        DCHECK(attrs_plane < arraysize(kLinuxDrmModifiers));
        attrs.push_back(kLinuxDrmModifiers[attrs_plane]);
        attrs.push_back(modifier & 0xffffffff);
        attrs.push_back(kLinuxDrmModifiers[attrs_plane] + 1);
        attrs.push_back(static_cast<uint32_t>(modifier >> 32));
      }
    }
    attrs.push_back(EGL_NONE);

    if (!GLImageEGL::Initialize(EGL_LINUX_DMA_BUF_EXT,
                                static_cast<EGLClientBuffer>(nullptr),
                                &attrs[0])) {
      return false;
    }
  }

  pixmap_ = pixmap;
  return true;
}

unsigned GLImageOzoneNativePixmap::GetInternalFormat() {
  return internalformat_;
}

bool GLImageOzoneNativePixmap::CopyTexImage(unsigned target) {
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    // Pass-through image type fails to bind and copy; make sure we
    // don't draw with uninitialized texture.
    std::vector<unsigned char> data(size_.width() * size_.height() * 4);
    glTexImage2D(target, 0, GL_RGBA, size_.width(), size_.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data.data());
    return true;
  }
  return GLImageEGL::CopyTexImage(target);
}

bool GLImageOzoneNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  DCHECK(pixmap_);
  return pixmap_->ScheduleOverlayPlane(widget, z_order, transform, bounds_rect,
                                       crop_rect);
}

void GLImageOzoneNativePixmap::Flush() {
  if (!has_image_flush_external_)
    return;

  EGLDisplay display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLAttrib attribs[] = {
      EGL_NONE,
  };
  if (!eglImageFlushExternalEXT(display, egl_image_, attribs)) {
    LOG(ERROR) << "Failed to flush rendering";
    return;
  }
}

void GLImageOzoneNativePixmap::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

// static
unsigned GLImageOzoneNativePixmap::GetInternalFormatForTesting(
    gfx::BufferFormat format) {
  DCHECK(ValidFormat(format));
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG_EXT;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return GL_NONE;
  }

  NOTREACHED();
  return GL_NONE;
}

}  // namespace ui
