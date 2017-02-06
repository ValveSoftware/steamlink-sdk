// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TRANSFORM_FEEDBACK_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TRANSFORM_FEEDBACK_MANAGER_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/indexed_buffer_binding_host.h"
#include "gpu/gpu_export.h"

namespace gfx {
struct GLVersionInfo;
};

namespace gpu {
namespace gles2 {

class Buffer;
class TransformFeedbackManager;

// Info about TransformFeedbacks currently in the system.
class GPU_EXPORT TransformFeedback : public IndexedBufferBindingHost {
 public:
  TransformFeedback(TransformFeedbackManager* manager, GLuint service_id);

  // All the following functions do state update and call the underlying GL
  // function.  All validations have been done already and the GL function is
  // guaranteed to succeed.
  void DoBindTransformFeedback(GLenum target);
  void DoBeginTransformFeedback(GLenum primitive_mode);
  void DoEndTransformFeedback();
  void DoPauseTransformFeedback();
  void DoResumeTransformFeedback();

  GLuint service_id() const {
    return service_id_;
  }

  bool has_been_bound() const {
    return has_been_bound_;
  }

  bool active() const {
    return active_;
  }

  bool paused() const {
    return paused_;
  }

  GLenum primitive_mode() const {
    return primitive_mode_;
  }

 private:
  ~TransformFeedback() override;

  // The manager that owns this Buffer.
  TransformFeedbackManager* manager_;

  GLuint service_id_;

  bool has_been_bound_;

  bool active_;
  bool paused_;

  GLenum primitive_mode_;
};

// This class keeps tracks of the transform feedbacks and their states.
class GPU_EXPORT TransformFeedbackManager {
 public:
  // |needs_emulation| is true on Desktop GL 4.1 or lower.
  TransformFeedbackManager(GLuint max_transform_feedback_separate_attribs,
                           bool needs_emulation);
  ~TransformFeedbackManager();

  void MarkContextLost() {
    lost_context_ = true;
  }

  // Must call before destruction.
  void Destroy();

  // Creates a TransformFeedback from the given client/service IDs and
  // insert it into the list.
  TransformFeedback* CreateTransformFeedback(
      GLuint client_id, GLuint service_id);

  TransformFeedback* GetTransformFeedback(GLuint client_id);

  // Removes a TransformFeedback info for the given client ID.
  void RemoveTransformFeedback(GLuint client_id);

  void RemoveBoundBuffer(Buffer* buffer);

  GLuint max_transform_feedback_separate_attribs() const {
    return max_transform_feedback_separate_attribs_;
  }

  bool needs_emulation() const {
    return needs_emulation_;
  }

  bool lost_context() const {
    return lost_context_;
  }

 private:
  // Info for each transform feedback in the system.
  base::hash_map<GLuint,
                 scoped_refptr<TransformFeedback> > transform_feedbacks_;

  GLuint max_transform_feedback_separate_attribs_;

  bool needs_emulation_;
  bool lost_context_;

  DISALLOW_COPY_AND_ASSIGN(TransformFeedbackManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TRANSFORM_FEEDBACK_MANAGER_H_
