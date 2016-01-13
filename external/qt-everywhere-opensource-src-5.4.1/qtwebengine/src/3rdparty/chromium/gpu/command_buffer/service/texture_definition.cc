// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_definition.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

#if !defined(OS_MACOSX)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

class GLImageSync : public gfx::GLImage {
 public:
  explicit GLImageSync(const scoped_refptr<NativeImageBuffer>& buffer,
                       const gfx::Size& size);

  // Implement GLImage.
  virtual void Destroy() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE;
  virtual void WillModifyTexImage() OVERRIDE;
  virtual void DidModifyTexImage() OVERRIDE;

  virtual void DidUseTexImage() OVERRIDE;
  virtual void SetReleaseAfterUse() OVERRIDE;

 protected:
  virtual ~GLImageSync();

 private:
  scoped_refptr<NativeImageBuffer> buffer_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GLImageSync);
};

GLImageSync::GLImageSync(const scoped_refptr<NativeImageBuffer>& buffer,
                         const gfx::Size& size)
    : buffer_(buffer), size_(size) {
  if (buffer)
    buffer->AddClient(this);
}

GLImageSync::~GLImageSync() {
  if (buffer_)
    buffer_->RemoveClient(this);
}

void GLImageSync::Destroy() {}

gfx::Size GLImageSync::GetSize() {
  return size_;
}

bool GLImageSync::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

void GLImageSync::ReleaseTexImage(unsigned target) {
  NOTREACHED();
}

void GLImageSync::WillUseTexImage() {
  if (buffer_)
    buffer_->WillRead(this);
}

void GLImageSync::DidUseTexImage() {
  if (buffer_)
    buffer_->DidRead(this);
}

void GLImageSync::WillModifyTexImage() {
  if (buffer_)
    buffer_->WillWrite(this);
}

void GLImageSync::DidModifyTexImage() {
  if (buffer_)
    buffer_->DidWrite(this);
}

void GLImageSync::SetReleaseAfterUse() {
  NOTREACHED();
}

#if !defined(OS_MACOSX)
class NativeImageBufferEGL : public NativeImageBuffer {
 public:
  static scoped_refptr<NativeImageBufferEGL> Create(GLuint texture_id);

 private:
  NativeImageBufferEGL(scoped_ptr<gfx::GLFence> write_fence,
                       EGLDisplay display,
                       EGLImageKHR image);
  virtual ~NativeImageBufferEGL();
  virtual void BindToTexture(GLenum target) OVERRIDE;

  EGLDisplay egl_display_;
  EGLImageKHR egl_image_;

  DISALLOW_COPY_AND_ASSIGN(NativeImageBufferEGL);
};

scoped_refptr<NativeImageBufferEGL> NativeImageBufferEGL::Create(
    GLuint texture_id) {
  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  EGLContext egl_context = eglGetCurrentContext();

  DCHECK_NE(EGL_NO_CONTEXT, egl_context);
  DCHECK_NE(EGL_NO_DISPLAY, egl_display);
  DCHECK(glIsTexture(texture_id));

  DCHECK(gfx::g_driver_egl.ext.b_EGL_KHR_image_base &&
         gfx::g_driver_egl.ext.b_EGL_KHR_gl_texture_2D_image &&
         gfx::g_driver_gl.ext.b_GL_OES_EGL_image &&
         gfx::g_driver_egl.ext.b_EGL_KHR_fence_sync);

  const EGLint egl_attrib_list[] = {
      EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  EGLClientBuffer egl_buffer = reinterpret_cast<EGLClientBuffer>(texture_id);
  EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR; // TODO

  EGLImageKHR egl_image = eglCreateImageKHR(
      egl_display, egl_context, egl_target, egl_buffer, egl_attrib_list);

  if (egl_image == EGL_NO_IMAGE_KHR)
    return NULL;

  return new NativeImageBufferEGL(
      make_scoped_ptr(gfx::GLFence::Create()), egl_display, egl_image);
}

NativeImageBufferEGL::NativeImageBufferEGL(scoped_ptr<gfx::GLFence> write_fence,
                                           EGLDisplay display,
                                           EGLImageKHR image)
    : NativeImageBuffer(write_fence.Pass()),
      egl_display_(display),
      egl_image_(image) {
  DCHECK(egl_display_ != EGL_NO_DISPLAY);
  DCHECK(egl_image_ != EGL_NO_IMAGE_KHR);
}

NativeImageBufferEGL::~NativeImageBufferEGL() {
  if (egl_image_ != EGL_NO_IMAGE_KHR)
    eglDestroyImageKHR(egl_display_, egl_image_);
}

void NativeImageBufferEGL::BindToTexture(GLenum target) {
  DCHECK(egl_image_ != EGL_NO_IMAGE_KHR);
  glEGLImageTargetTexture2DOES(target, egl_image_);
  DCHECK_EQ(static_cast<EGLint>(EGL_SUCCESS), eglGetError());
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}
#endif

class NativeImageBufferStub : public NativeImageBuffer {
 public:
  NativeImageBufferStub() : NativeImageBuffer(scoped_ptr<gfx::GLFence>()) {}

 private:
  virtual ~NativeImageBufferStub() {}
  virtual void BindToTexture(GLenum target) OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(NativeImageBufferStub);
};

}  // anonymous namespace

// static
scoped_refptr<NativeImageBuffer> NativeImageBuffer::Create(GLuint texture_id) {
  switch (gfx::GetGLImplementation()) {
#if !defined(OS_MACOSX)
    case gfx::kGLImplementationEGLGLES2:
      return NativeImageBufferEGL::Create(texture_id);
#endif
    case gfx::kGLImplementationMockGL:
      return new NativeImageBufferStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

NativeImageBuffer::ClientInfo::ClientInfo(gfx::GLImage* client)
    : client(client), needs_wait_before_read(true) {}

NativeImageBuffer::ClientInfo::~ClientInfo() {}

NativeImageBuffer::NativeImageBuffer(scoped_ptr<gfx::GLFence> write_fence)
    : write_fence_(write_fence.Pass()), write_client_(NULL) {
}

NativeImageBuffer::~NativeImageBuffer() {
  DCHECK(client_infos_.empty());
}

void NativeImageBuffer::AddClient(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  client_infos_.push_back(ClientInfo(client));
}

void NativeImageBuffer::RemoveClient(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  if (write_client_ == client)
    write_client_ = NULL;
  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    if (it->client == client) {
      client_infos_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

bool NativeImageBuffer::IsClient(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    if (it->client == client)
      return true;
  }
  return false;
}

void NativeImageBuffer::WillRead(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  if (!write_fence_.get() || write_client_ == client)
    return;

  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    if (it->client == client) {
      if (it->needs_wait_before_read) {
        it->needs_wait_before_read = false;
        write_fence_->ServerWait();
      }
      return;
    }
  }
  NOTREACHED();
}

void NativeImageBuffer::WillWrite(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  if (write_client_ != client)
    write_fence_->ServerWait();

  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    if (it->read_fence.get() && it->client != client)
      it->read_fence->ServerWait();
  }
}

void NativeImageBuffer::DidRead(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    if (it->client == client) {
      it->read_fence = make_linked_ptr(gfx::GLFence::Create());
      return;
    }
  }
  NOTREACHED();
}

void NativeImageBuffer::DidWrite(gfx::GLImage* client) {
  base::AutoLock lock(lock_);
  // Sharing semantics require the client to flush in order to make changes
  // visible to other clients.
  write_fence_.reset(gfx::GLFence::CreateWithoutFlush());
  write_client_ = client;
  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end();
       it++) {
    it->needs_wait_before_read = true;
  }
}

TextureDefinition::LevelInfo::LevelInfo(GLenum target,
                                        GLenum internal_format,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLenum format,
                                        GLenum type,
                                        bool cleared)
    : target(target),
      internal_format(internal_format),
      width(width),
      height(height),
      depth(depth),
      border(border),
      format(format),
      type(type),
      cleared(cleared) {}

TextureDefinition::LevelInfo::~LevelInfo() {}

TextureDefinition::TextureDefinition(
    GLenum target,
    Texture* texture,
    unsigned int version,
    const scoped_refptr<NativeImageBuffer>& image_buffer)
    : version_(version),
      target_(target),
      image_buffer_(image_buffer ? image_buffer : NativeImageBuffer::Create(
                                                      texture->service_id())),
      min_filter_(texture->min_filter()),
      mag_filter_(texture->mag_filter()),
      wrap_s_(texture->wrap_s()),
      wrap_t_(texture->wrap_t()),
      usage_(texture->usage()),
      immutable_(texture->IsImmutable()) {

  // TODO
  DCHECK(!texture->level_infos_.empty());
  DCHECK(!texture->level_infos_[0].empty());
  DCHECK(!texture->NeedsMips());
  DCHECK(texture->level_infos_[0][0].width);
  DCHECK(texture->level_infos_[0][0].height);

  scoped_refptr<gfx::GLImage> gl_image(
      new GLImageSync(image_buffer_,
                      gfx::Size(texture->level_infos_[0][0].width,
                                texture->level_infos_[0][0].height)));
  texture->SetLevelImage(NULL, target, 0, gl_image);

  // TODO: all levels
  level_infos_.clear();
  const Texture::LevelInfo& level = texture->level_infos_[0][0];
  LevelInfo info(level.target,
                 level.internal_format,
                 level.width,
                 level.height,
                 level.depth,
                 level.border,
                 level.format,
                 level.type,
                 level.cleared);
  std::vector<LevelInfo> infos;
  infos.push_back(info);
  level_infos_.push_back(infos);
}

TextureDefinition::~TextureDefinition() {
}

Texture* TextureDefinition::CreateTexture() const {
  if (!image_buffer_)
    return NULL;

  GLuint texture_id;
  glGenTextures(1, &texture_id);

  Texture* texture(new Texture(texture_id));
  UpdateTexture(texture);

  return texture;
}

void TextureDefinition::UpdateTexture(Texture* texture) const {
  gfx::ScopedTextureBinder texture_binder(target_, texture->service_id());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t_);
  if (image_buffer_)
    image_buffer_->BindToTexture(target_);
  // We have to make sure the changes are visible to other clients in this share
  // group. As far as the clients are concerned, the mailbox semantics only
  // demand a single flush from the client after changes are first made,
  // and it is not visible to them when another share group boundary is crossed.
  // We could probably track this and be a bit smarter about when to flush
  // though.
  glFlush();

  texture->level_infos_.resize(1);
  for (size_t i = 0; i < level_infos_.size(); i++) {
    const LevelInfo& base_info = level_infos_[i][0];
    const size_t levels_needed = TextureManager::ComputeMipMapCount(
        base_info.target, base_info.width, base_info.height, base_info.depth);
    DCHECK(level_infos_.size() <= levels_needed);
    texture->level_infos_[0].resize(levels_needed);
    for (size_t n = 0; n < level_infos_.size(); n++) {
      const LevelInfo& info = level_infos_[i][n];
      texture->SetLevelInfo(NULL,
                            info.target,
                            i,
                            info.internal_format,
                            info.width,
                            info.height,
                            info.depth,
                            info.border,
                            info.format,
                            info.type,
                            info.cleared);
    }
  }
  if (image_buffer_) {
    texture->SetLevelImage(
        NULL,
        target_,
        0,
        new GLImageSync(
            image_buffer_,
            gfx::Size(level_infos_[0][0].width, level_infos_[0][0].height)));
  }

  texture->target_ = target_;
  texture->SetImmutable(immutable_);
  texture->min_filter_ = min_filter_;
  texture->mag_filter_ = mag_filter_;
  texture->wrap_s_ = wrap_s_;
  texture->wrap_t_ = wrap_t_;
  texture->usage_ = usage_;
}

bool TextureDefinition::Matches(const Texture* texture) const {
  DCHECK(target_ == texture->target());
  if (texture->min_filter_ != min_filter_ ||
      texture->mag_filter_ != mag_filter_ ||
      texture->wrap_s_ != wrap_s_ ||
      texture->wrap_t_ != wrap_t_) {
    return false;
  }

  // All structural changes should have orphaned the texture.
  if (image_buffer_ && !texture->GetLevelImage(texture->target(), 0))
    return false;

  return true;
}

}  // namespace gles2
}  // namespace gpu
