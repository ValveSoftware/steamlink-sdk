// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_BUFFER_H_
#define COMPONENTS_EXO_BUFFER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {
class SingleReleaseCallback;
class TextureMailbox;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
struct SyncToken;
}

namespace exo {

// This class provides the content for a Surface. The mechanism by which a
// client provides and updates the contents is the responsibility of the client
// and not defined as part of this class.
class Buffer : public base::SupportsWeakPtr<Buffer> {
 public:
  explicit Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer);
  Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
         unsigned texture_target,
         unsigned query_type,
         bool use_zero_copy,
         bool is_overlay_candidate);
  ~Buffer();

  // Set the callback to run when the buffer is no longer used by the
  // compositor. The client is free to re-use or destroy this buffer and
  // its backing storage after this has been called.
  void set_release_callback(const base::Closure& release_callback) {
    release_callback_ = release_callback;
  }

  // This function can be used to acquire a texture mailbox for the contents of
  // buffer. Returns a release callback on success. The release callback should
  // be called before a new texture mailbox can be acquired unless
  // |non_client_usage| is true.
  std::unique_ptr<cc::SingleReleaseCallback> ProduceTextureMailbox(
      cc::TextureMailbox* mailbox,
      bool secure_output_only,
      bool client_usage);

  // This should be called when the buffer is attached to a Surface.
  void OnAttach();

  // This should be called when the buffer is detached from a surface.
  void OnDetach();

  // Returns the size of the buffer.
  gfx::Size GetSize() const;

  // Returns a trace value representing the state of the buffer.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

 private:
  class Texture;

  // Decrements the use count of buffer and notifies the client that buffer
  // as been released if it reached 0.
  void Release();

  // Runs the release callback if the buffer isn't attached or in use.
  void CheckReleaseCallback();

  // This is used by ProduceTextureMailbox() to produce a release callback
  // that releases a texture so it can be destroyed or reused.
  void ReleaseTexture(std::unique_ptr<Texture> texture);

  // This is used by ProduceTextureMailbox() to produce a release callback
  // that releases the buffer contents referenced by a texture before the
  // texture is destroyed or reused.
  void ReleaseContentsTexture(std::unique_ptr<Texture> texture);

  // The GPU memory buffer that contains the contents of this buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // Texture target that must be used when creating a texture for buffer.
  const unsigned texture_target_;

  // Query type that must be used when releasing buffer from a texture.
  const unsigned query_type_;

  // True if zero copy is used when producing a texture mailbox for buffer.
  const bool use_zero_copy_;

  // True if this buffer is an overlay candidate.
  const bool is_overlay_candidate_;

  // This is incremented when a texture mailbox is produced and decremented
  // when a texture mailbox is released. It is used to determine when we should
  // notify the client that buffer has been released.
  unsigned use_count_ = 0;

  // This keeps track of how many Surfaces the buffer is attached to.
  unsigned attach_count_ = 0;

  // The last used texture. ProduceTextureMailbox() will use this
  // instead of creating a new texture when possible.
  std::unique_ptr<Texture> texture_;

  // The last used contents texture. ProduceTextureMailbox() will use this
  // instead of creating a new texture when possible.
  std::unique_ptr<Texture> contents_texture_;

  // The client release callback.
  base::Closure release_callback_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_BUFFER_H_
