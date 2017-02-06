// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace crypto {
class SymmetricKey;
}

namespace chromecast {
namespace media {

class DecoderBufferBase;
class DecryptContextImpl;

using BufferDecryptedCB =
    base::Callback<void(scoped_refptr<DecoderBufferBase>, bool)>;

// Create a new buffer which corresponds to the clear version of |buffer|.
// Note: the memory area corresponding to the ES data of the new buffer
// is the same as the ES data of |buffer| (for efficiency).
// After the |buffer_decrypted_cb| is called, |buffer| is left in a inconsistent
// state in the sense it has some decryption info but the ES data is now in
// clear.
void DecryptDecoderBuffer(scoped_refptr<DecoderBufferBase> buffer,
                          DecryptContextImpl* decrypt_ctxt,
                          const BufferDecryptedCB& buffer_decrypted_cb);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
