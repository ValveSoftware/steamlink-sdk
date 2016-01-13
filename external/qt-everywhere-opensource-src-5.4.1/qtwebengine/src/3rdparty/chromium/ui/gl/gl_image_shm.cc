// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shm.h"

#include "base/debug/trace_event.h"
#include "base/process/process_handle.h"
#include "ui/gl/scoped_binders.h"

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gfx {

namespace {

bool ValidFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

GLenum TextureFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return GL_BGRA_EXT;
    case GL_RGBA8_OES:
      return GL_RGBA;
    default:
      NOTREACHED();
      return 0;
  }
}

GLenum DataFormat(unsigned internalformat) {
  return TextureFormat(internalformat);
}

GLenum DataType(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return GL_UNSIGNED_BYTE;
    default:
      NOTREACHED();
      return 0;
  }
}

GLenum BytesPerPixel(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

GLImageShm::GLImageShm(gfx::Size size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat)
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
      ,
      egl_texture_id_(0u),
      egl_image_(EGL_NO_IMAGE_KHR)
#endif
{
}

GLImageShm::~GLImageShm() { Destroy(); }

bool GLImageShm::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  if (!ValidFormat(internalformat_)) {
    DVLOG(0) << "Invalid format: " << internalformat_;
    return false;
  }

  if (!base::SharedMemory::IsHandleValid(buffer.handle))
    return false;

  base::SharedMemory shared_memory(buffer.handle, true);

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                    &duped_shared_memory_handle)) {
    DVLOG(0) << "Failed to duplicate shared memory handle.";
    return false;
  }

  shared_memory_.reset(
      new base::SharedMemory(duped_shared_memory_handle, true));
  return true;
}

void GLImageShm::Destroy() {
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_);
    egl_image_ = EGL_NO_IMAGE_KHR;
  }

  if (egl_texture_id_) {
    glDeleteTextures(1, &egl_texture_id_);
    egl_texture_id_ = 0u;
  }
#endif
}

gfx::Size GLImageShm::GetSize() { return size_; }

bool GLImageShm::BindTexImage(unsigned target) {
  TRACE_EVENT0("gpu", "GLImageShm::BindTexImage");
  DCHECK(shared_memory_);
  DCHECK(ValidFormat(internalformat_));

  size_t size = size_.GetArea() * BytesPerPixel(internalformat_);
  DCHECK(!shared_memory_->memory());
  if (!shared_memory_->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  DCHECK(shared_memory_->memory());

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (target == GL_TEXTURE_EXTERNAL_OES) {
    if (egl_image_ == EGL_NO_IMAGE_KHR) {
      DCHECK_EQ(0u, egl_texture_id_);
      glGenTextures(1, &egl_texture_id_);

      {
        ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,  // mip level
                     TextureFormat(internalformat_),
                     size_.width(),
                     size_.height(),
                     0,  // border
                     DataFormat(internalformat_),
                     DataType(internalformat_),
                     shared_memory_->memory());
      }

      EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
      // Need to pass current EGL rendering context to eglCreateImageKHR for
      // target type EGL_GL_TEXTURE_2D_KHR.
      egl_image_ =
          eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(),
                            eglGetCurrentContext(),
                            EGL_GL_TEXTURE_2D_KHR,
                            reinterpret_cast<EGLClientBuffer>(egl_texture_id_),
                            attrs);
      DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_)
          << "Error creating EGLImage: " << eglGetError();
    } else {
      ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

      glTexSubImage2D(GL_TEXTURE_2D,
                      0,  // mip level
                      0,  // x-offset
                      0,  // y-offset
                      size_.width(),
                      size_.height(),
                      DataFormat(internalformat_),
                      DataType(internalformat_),
                      shared_memory_->memory());
    }

    glEGLImageTargetTexture2DOES(target, egl_image_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    shared_memory_->Unmap();
    return true;
  }
#endif

  DCHECK_NE(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), target);
  glTexImage2D(target,
               0,  // mip level
               TextureFormat(internalformat_),
               size_.width(),
               size_.height(),
               0,  // border
               DataFormat(internalformat_),
               DataType(internalformat_),
               shared_memory_->memory());

  shared_memory_->Unmap();
  return true;
}

}  // namespace gfx
