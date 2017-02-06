// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBP_TRANSCODE_WEBP_DECODER_H_
#define COMPONENTS_WEBP_TRANSCODE_WEBP_DECODER_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "third_party/libwebp/webp/decode.h"

@class NSData;

namespace webp_transcode {

// Decodes a WebP image into either JPEG, PNG or uncompressed TIFF.
class WebpDecoder : public base::RefCountedThreadSafe<WebpDecoder> {
 public:
  // Format of the decoded image.
  // This enum is used for UMA reporting, keep it in sync with the histogram
  // definition.
  enum DecodedImageFormat { JPEG = 1, PNG, TIFF, DECODED_FORMAT_COUNT };

  class Delegate : public base::RefCountedThreadSafe<WebpDecoder::Delegate> {
   public:
    virtual void OnFinishedDecoding(bool success) = 0;
    virtual void SetImageFeatures(size_t total_size,  // In bytes.
                                  DecodedImageFormat format) = 0;
    virtual void OnDataDecoded(NSData* data) = 0;

   protected:
    friend class base::RefCountedThreadSafe<WebpDecoder::Delegate>;
    virtual ~Delegate() {}
  };

  explicit WebpDecoder(WebpDecoder::Delegate* delegate);

  // For tests.
  static size_t GetHeaderSize();

  // Main entry point.
  void OnDataReceived(const base::scoped_nsobject<NSData>& data);

  // Stops the decoding.
  void Stop();

 private:
  struct WebPIDecoderDeleter {
    inline void operator()(WebPIDecoder* ptr) const { WebPIDelete(ptr); }
  };

  enum State { READING_FEATURES, READING_DATA, DONE };

  friend class base::RefCountedThreadSafe<WebpDecoder>;
  virtual ~WebpDecoder();

  // Implements WebP image decoding state machine steps.
  void DoReadFeatures(NSData* data);
  void DoReadData(NSData* data);
  bool DoSendData();

  scoped_refptr<WebpDecoder::Delegate> delegate_;
  WebPDecoderConfig config_;
  WebpDecoder::State state_;
  std::unique_ptr<WebPIDecoder, WebPIDecoderDeleter> incremental_decoder_;
  base::scoped_nsobject<NSData> output_buffer_;
  base::scoped_nsobject<NSMutableData> features_;
  int has_alpha_;
};

}  // namespace webp_transcode

#endif  // COMPONENTS_WEBP_TRANSCODE_WEBP_DECODER_H_
