// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_

#include <list>
#include <vector>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gl/gl_fence.h"

namespace gfx {
class GLFence;
class GLImage;
}

namespace gpu {
namespace gles2 {

class Texture;

class NativeImageBuffer : public base::RefCountedThreadSafe<NativeImageBuffer> {
 public:
  static scoped_refptr<NativeImageBuffer> Create(GLuint texture_id);
  virtual void BindToTexture(GLenum target) = 0;

  void AddClient(gfx::GLImage* client);
  void RemoveClient(gfx::GLImage* client);
  bool IsClient(gfx::GLImage* client);

  void WillRead(gfx::GLImage* client);
  void WillWrite(gfx::GLImage* client);
  void DidRead(gfx::GLImage* client);
  void DidWrite(gfx::GLImage* client);

 protected:
  friend class base::RefCountedThreadSafe<NativeImageBuffer>;
  explicit NativeImageBuffer(scoped_ptr<gfx::GLFence> write_fence);
  virtual ~NativeImageBuffer();

  base::Lock lock_;

  struct ClientInfo {
    ClientInfo(gfx::GLImage* client);
    ~ClientInfo();

    gfx::GLImage* client;
    bool needs_wait_before_read;
    linked_ptr<gfx::GLFence> read_fence;
  };
  std::list<ClientInfo> client_infos_;
  scoped_ptr<gfx::GLFence> write_fence_;
  gfx::GLImage* write_client_;

  DISALLOW_COPY_AND_ASSIGN(NativeImageBuffer);
};

// An immutable description that can be used to create a texture that shares
// the underlying image buffer(s).
class TextureDefinition {
 public:
  TextureDefinition(GLenum target,
                    Texture* texture,
                    unsigned int version,
                    const scoped_refptr<NativeImageBuffer>& image);
  virtual ~TextureDefinition();

  Texture* CreateTexture() const;
  void UpdateTexture(Texture* texture) const;

  unsigned int version() const { return version_; }
  bool IsOlderThan(unsigned int version) const {
    return (version - version_) < 0x80000000;
  }
  bool Matches(const Texture* texture) const;

  scoped_refptr<NativeImageBuffer> image() { return image_buffer_; }

 private:
  struct LevelInfo {
    LevelInfo(GLenum target,
              GLenum internal_format,
              GLsizei width,
              GLsizei height,
              GLsizei depth,
              GLint border,
              GLenum format,
              GLenum type,
              bool cleared);
    ~LevelInfo();

    GLenum target;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum format;
    GLenum type;
    bool cleared;
  };

  typedef std::vector<std::vector<LevelInfo> > LevelInfos;

  unsigned int version_;
  GLenum target_;
  scoped_refptr<NativeImageBuffer> image_buffer_;
  GLenum min_filter_;
  GLenum mag_filter_;
  GLenum wrap_s_;
  GLenum wrap_t_;
  GLenum usage_;
  bool immutable_;
  LevelInfos level_infos_;
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_
