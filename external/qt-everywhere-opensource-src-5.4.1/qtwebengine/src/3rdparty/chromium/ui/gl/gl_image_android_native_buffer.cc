// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_android_native_buffer.h"

#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace gfx {

GLImageAndroidNativeBuffer::GLImageAndroidNativeBuffer(gfx::Size size)
    : GLImageEGL(size),
      release_after_use_(false),
      in_use_(false),
      target_(0),
      egl_image_for_unbind_(EGL_NO_IMAGE_KHR),
      texture_id_for_unbind_(0) {}

GLImageAndroidNativeBuffer::~GLImageAndroidNativeBuffer() { Destroy(); }

bool GLImageAndroidNativeBuffer::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  DCHECK(buffer.native_buffer);

  EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  return GLImageEGL::Initialize(
      EGL_NATIVE_BUFFER_ANDROID, buffer.native_buffer, attrs);
}

void GLImageAndroidNativeBuffer::Destroy() {
  if (egl_image_for_unbind_ != EGL_NO_IMAGE_KHR) {
    eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(),
                       egl_image_for_unbind_);
    egl_image_for_unbind_ = EGL_NO_IMAGE_KHR;
  }
  if (texture_id_for_unbind_) {
    glDeleteTextures(1, &texture_id_for_unbind_);
    texture_id_for_unbind_ = 0;
  }

  GLImageEGL::Destroy();
}

bool GLImageAndroidNativeBuffer::BindTexImage(unsigned target) {
  DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_);

  if (target == GL_TEXTURE_RECTANGLE_ARB) {
    LOG(ERROR) << "EGLImage cannot be bound to TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  if (target_ && target_ != target) {
    LOG(ERROR) << "EGLImage can only be bound to one target";
    return false;
  }
  target_ = target;

  // Defer ImageTargetTexture2D if not currently in use.
  if (!in_use_)
    return true;

  glEGLImageTargetTexture2DOES(target_, egl_image_);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  return true;
}

void GLImageAndroidNativeBuffer::WillUseTexImage() {
  DCHECK(egl_image_);
  DCHECK(!in_use_);
  in_use_ = true;
  glEGLImageTargetTexture2DOES(target_, egl_image_);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

void GLImageAndroidNativeBuffer::DidUseTexImage() {
  DCHECK(in_use_);
  in_use_ = false;

  if (!release_after_use_)
    return;

  if (egl_image_for_unbind_ == EGL_NO_IMAGE_KHR) {
    DCHECK_EQ(0u, texture_id_for_unbind_);
    glGenTextures(1, &texture_id_for_unbind_);

    {
      ScopedTextureBinder texture_binder(GL_TEXTURE_2D, texture_id_for_unbind_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      char zero[4] = {0, };
      glTexImage2D(
          GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &zero);
    }

    EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    // Need to pass current EGL rendering context to eglCreateImageKHR for
    // target type EGL_GL_TEXTURE_2D_KHR.
    egl_image_for_unbind_ = eglCreateImageKHR(
        GLSurfaceEGL::GetHardwareDisplay(),
        eglGetCurrentContext(),
        EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(texture_id_for_unbind_),
        attrs);
    DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_for_unbind_)
        << "Error creating EGLImage: " << eglGetError();
  }

  glEGLImageTargetTexture2DOES(target_, egl_image_for_unbind_);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

void GLImageAndroidNativeBuffer::SetReleaseAfterUse() {
  release_after_use_ = true;
}

}  // namespace gfx
