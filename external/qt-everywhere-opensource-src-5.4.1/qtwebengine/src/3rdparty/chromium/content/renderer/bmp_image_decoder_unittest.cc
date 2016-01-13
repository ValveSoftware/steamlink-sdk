// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/image_decoder_test.h"
#include "third_party/WebKit/public/web/WebImageDecoder.h"

class BMPImageDecoderTest : public ImageDecoderTest {
 public:
  BMPImageDecoderTest() : ImageDecoderTest("bmp") { }

 protected:
  virtual blink::WebImageDecoder* CreateWebKitImageDecoder() const OVERRIDE {
    return new blink::WebImageDecoder(blink::WebImageDecoder::TypeBMP);
  }

  // The BMPImageDecoderTest tests are really slow under Valgrind.
  // Thus it is split into fast and slow versions. The threshold is
  // set to 10KB because the fast test can finish under Valgrind in
  // less than 30 seconds.
  static const int64 kThresholdSize = 10240;
};

TEST_F(BMPImageDecoderTest, DecodingFast) {
  TestDecoding(TEST_SMALLER, kThresholdSize);
}

#if defined(THREAD_SANITIZER)
// BMPImageDecoderTest.DecodingSlow always times out under ThreadSanitizer v2.
#define MAYBE_DecodingSlow DISABLED_DecodingSlow
#else
#define MAYBE_DecodingSlow DecodingSlow
#endif
TEST_F(BMPImageDecoderTest, MAYBE_DecodingSlow) {
  TestDecoding(TEST_BIGGER, kThresholdSize);
}
