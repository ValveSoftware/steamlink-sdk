// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_
#define GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class Buffer;

// This is a base class for indexed buffer bindings tracking.
// TransformFeedback and Program should inherit from this base class,
// for tracking indexed TRANSFORM_FEEDBACK_BUFFER / UNIFORM_BUFFER bindings.
class GPU_EXPORT IndexedBufferBindingHost :
    public base::RefCounted<IndexedBufferBindingHost> {
 public:
  // |needs_emulation| is set to true on Desktop GL 4.1 or lower.
  IndexedBufferBindingHost(uint32_t max_bindings, bool needs_emulation);

  // The following two functions do state update and call the underlying GL
  // function.  All validations have been done already and the GL function is
  // guaranteed to succeed.
  void DoBindBufferBase(GLenum target, GLuint index, Buffer* buffer);
  void DoBindBufferRange(
      GLenum target, GLuint index, Buffer* buffer, GLintptr offset,
      GLsizeiptr size);

  // This is called on the active host when glBufferData is called and buffer
  // size might change.
  void OnBufferData(GLenum target, Buffer* buffer);

  // This is called when the host become active.
  void OnBindHost(GLenum target);

  void RemoveBoundBuffer(Buffer* buffer);

  Buffer* GetBufferBinding(GLuint index) const;
  GLsizeiptr GetBufferSize(GLuint index) const;
  GLintptr GetBufferStart(GLuint index) const;

  // This is used only for UNIFORM_BUFFER bindings in context switching.
  void RestoreBindings(IndexedBufferBindingHost* prev);

 protected:
  friend class base::RefCounted<IndexedBufferBindingHost>;

  virtual ~IndexedBufferBindingHost();

 private:
  enum IndexedBufferBindingType {
    kBindBufferBase,
    kBindBufferRange,
    kBindBufferNone
  };

  struct IndexedBufferBinding {
    IndexedBufferBindingType type;
    scoped_refptr<Buffer> buffer;

    // The following fields are only used if |type| is kBindBufferRange.
    GLintptr offset;
    GLsizeiptr size;
    // The full buffer size at the last successful glBindBufferRange call.
    GLsizeiptr effective_full_buffer_size;

    IndexedBufferBinding();
    IndexedBufferBinding(const IndexedBufferBinding& other);
    ~IndexedBufferBinding();

    bool operator==(const IndexedBufferBinding& other) const;

    void SetBindBufferBase(Buffer* _buffer);
    void SetBindBufferRange(
        Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);
    void Reset();
  };

  // This is called on Desktop GL lower than 4.2, where the range
  // (offset + size) can't go beyond the buffer's size.
  static void DoAdjustedBindBufferRange(
      GLenum target, GLuint index, GLuint service_id, GLintptr offset,
      GLsizeiptr size, GLsizeiptr full_buffer_size);

  void UpdateMaxNonNullBindingIndex(size_t changed_index);

  std::vector<IndexedBufferBinding> buffer_bindings_;

  bool needs_emulation_;

  // This is used for optimization purpose in context switching.
  size_t max_non_null_binding_index_plus_one_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_
