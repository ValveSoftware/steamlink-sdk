// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_PPAPI_CDM_BUFFER_H_
#define MEDIA_CDM_PPAPI_PPAPI_CDM_BUFFER_H_

#include <map>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"

namespace media {

class PpbBufferAllocator;

// cdm::Buffer implementation that provides access to memory owned by a
// pp::Buffer_Dev.
// This class holds a reference to the Buffer_Dev throughout its lifetime.
// TODO(xhwang): Find a better name. It's confusing to have PpbBuffer,
// pp::Buffer_Dev and PPB_Buffer_Dev.
class PpbBuffer : public cdm::Buffer {
 public:
  static PpbBuffer* Create(const pp::Buffer_Dev& buffer,
                           uint32_t buffer_id,
                           PpbBufferAllocator* allocator);

  // cdm::Buffer implementation.
  void Destroy() override;
  uint32_t Capacity() const override;
  uint8_t* Data() override;
  void SetSize(uint32_t size) override;
  uint32_t Size() const override { return size_; }

  // Takes the |buffer_| from this class and returns it.
  // Note: The caller must ensure |allocator->Release()| is called later so that
  // the buffer can be reused by the allocator.
  // Since pp::Buffer_Dev is ref-counted, the caller now holds one reference to
  // the buffer and this class holds no reference. Note that other references
  // may still exist. For example, PpbBufferAllocator always holds a reference
  // to all allocated buffers.
  pp::Buffer_Dev TakeBuffer();

  uint32_t buffer_id() const { return buffer_id_; }

 private:
  PpbBuffer(pp::Buffer_Dev buffer,
            uint32_t buffer_id,
            PpbBufferAllocator* allocator);
  ~PpbBuffer() override;

  pp::Buffer_Dev buffer_;
  uint32_t buffer_id_;
  uint32_t size_;
  PpbBufferAllocator* allocator_;

  DISALLOW_COPY_AND_ASSIGN(PpbBuffer);
};

class PpbBufferAllocator {
 public:
  explicit PpbBufferAllocator(pp::Instance* instance)
      : instance_(instance), next_buffer_id_(1) {}
  ~PpbBufferAllocator() {}

  cdm::Buffer* Allocate(uint32_t capacity);

  // Releases the buffer with |buffer_id|. A buffer can be recycled after
  // it is released.
  void Release(uint32_t buffer_id);

 private:
  typedef std::map<uint32_t, pp::Buffer_Dev> AllocatedBufferMap;
  typedef std::multimap<uint32_t, std::pair<uint32_t, pp::Buffer_Dev>>
      FreeBufferMap;

  pp::Buffer_Dev AllocateNewBuffer(uint32_t capacity);

  pp::Instance* const instance_;
  uint32_t next_buffer_id_;
  AllocatedBufferMap allocated_buffers_;
  FreeBufferMap free_buffers_;

  DISALLOW_COPY_AND_ASSIGN(PpbBufferAllocator);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_PPAPI_CDM_BUFFER_H_
