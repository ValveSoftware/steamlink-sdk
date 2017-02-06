// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLOB_IMAGE_SERIALIZATION_PROCESSOR_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLOB_IMAGE_SERIALIZATION_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "cc/blimp/image_serialization_processor.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace blimp {

class BlobChannelReceiver;

namespace client {

// Adds BlobChannel image retrieval support to the Skia image decoding process,
// in addition to providing a cache for Skia images.
class BlobImageSerializationProcessor : public cc::ImageSerializationProcessor {
 public:
  class ErrorDelegate {
   public:
    virtual void OnImageDecodeError() = 0;
  };

  // Returns the BlobImageSerializationProcessor* instance that is active
  // for the current process.
  static BlobImageSerializationProcessor* current();

  BlobImageSerializationProcessor();
  ~BlobImageSerializationProcessor() override;

  // Sets the |blob_receiver| to use for reading images.
  // |blob_receiver| must outlive |this|.
  void set_blob_receiver(BlobChannelReceiver* blob_receiver) {
    blob_receiver_ = blob_receiver;
  }

  // |error_delegate| must outlive this.
  void set_error_delegate(ErrorDelegate* error_delegate) {
    error_delegate_ = error_delegate;
  }

  // Retrieves a bitmap with ID |input| from |blob_receiver_| and decodes it
  // to |bitmap|.
  bool GetAndDecodeBlob(const void* input,
                        size_t input_size,
                        SkBitmap* bitmap) const;

 private:
  friend struct base::DefaultSingletonTraits<BlobImageSerializationProcessor>;

  // Adapts a bare function pointer call to a singleton call to
  // GetAndDecodeBlob() call on the current() processor.
  static bool InstallPixelRefProc(const void* input,
                                  size_t input_size,
                                  SkBitmap* bitmap);

  // cc:ImageSerializationProcessor implementation.
  std::unique_ptr<cc::EnginePictureCache> CreateEnginePictureCache() override;
  std::unique_ptr<cc::ClientPictureCache> CreateClientPictureCache() override;

  // Interface for accessing stored images received over the Blob Channel.
  BlobChannelReceiver* blob_receiver_ = nullptr;

  ErrorDelegate* error_delegate_ = nullptr;

  // Pointer is bound on object construction, and set to nullptr on object
  // deletion. Only one BlobImageSerializationProcessor may be live at a time.
  static BlobImageSerializationProcessor* current_instance_;

  DISALLOW_COPY_AND_ASSIGN(BlobImageSerializationProcessor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLOB_IMAGE_SERIALIZATION_PROCESSOR_H_
