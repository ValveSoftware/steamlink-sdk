// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_BITSTREAM_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace IPC {
template <class P>
struct ParamTraits;
}

namespace media {

// Class for passing bitstream buffers around.  Does not take ownership of the
// data.  This is the media-namespace equivalent of PP_VideoBitstreamBuffer_Dev.
class MEDIA_EXPORT BitstreamBuffer {
 public:
  BitstreamBuffer();

  // Constructs a new BitstreamBuffer. The content of the bitstream is located
  // at |offset| bytes away from the start of the shared memory and the payload
  // is |size| bytes. When not provided, the default value for |offset| is 0.
  // |presentation_timestamp| is when the decoded frame should be displayed.
  // When not provided, |presentation_timestamp| will be
  // |media::kNoTimestamp()|.
  BitstreamBuffer(int32_t id,
                  base::SharedMemoryHandle handle,
                  size_t size,
                  off_t offset = 0,
                  base::TimeDelta presentation_timestamp = kNoTimestamp());

  BitstreamBuffer(const BitstreamBuffer& other);

  ~BitstreamBuffer();

  void SetDecryptConfig(const DecryptConfig& decrypt_config);

  int32_t id() const { return id_; }
  base::SharedMemoryHandle handle() const { return handle_; }

  // The number of bytes of the actual bitstream data. It is the size of the
  // content instead of the whole shared memory.
  size_t size() const { return size_; }

  // The offset to the start of actual bitstream data in the shared memory.
  off_t offset() const { return offset_; }

  // The timestamp is only valid if it's not equal to |media::kNoTimestamp()|.
  base::TimeDelta presentation_timestamp() const {
    return presentation_timestamp_;
  }

  void set_handle(const base::SharedMemoryHandle& handle) { handle_ = handle; }

  // The following methods come from DecryptConfig.

  const std::string& key_id() const { return key_id_; }
  const std::string& iv() const { return iv_; }
  const std::vector<SubsampleEntry>& subsamples() const { return subsamples_; }

 private:
  int32_t id_;
  base::SharedMemoryHandle handle_;
  size_t size_;
  off_t offset_;

  // This is only set when necessary. For example, AndroidVideoDecodeAccelerator
  // needs the timestamp because the underlying decoder may require it to
  // determine the output order.
  base::TimeDelta presentation_timestamp_;

  // The following fields come from DecryptConfig.
  // TODO(timav): Try to DISALLOW_COPY_AND_ASSIGN and include these params as
  // std::unique_ptr<DecryptConfig> or explain why copy & assign is needed.

  std::string key_id_;                      // key ID.
  std::string iv_;                          // initialization vector
  std::vector<SubsampleEntry> subsamples_;  // clear/cypher sizes

  friend struct IPC::ParamTraits<media::BitstreamBuffer>;

  // Allow compiler-generated copy & assign constructors.
};

}  // namespace media

#endif  // MEDIA_BASE_BITSTREAM_BUFFER_H_
