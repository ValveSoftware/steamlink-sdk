// Copyright 2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This is branched from AOSP's frameworks/base/opengl/include/ETC1/etc1.h
//
// It has been modified to implement Chromium's ETC1 API.  The decoding path
// has been removed as we do not require it.

#ifndef THIRD_PARTY_ANDROID_OPENGL_ETC1_ETC1_H_
#define THIRD_PARTY_ANDROID_OPENGL_ETC1_ETC1_H_

// Return the size of the encoded image data.

unsigned int etc1_get_encoded_data_size(unsigned int width, unsigned int height);

// Encode an entire image.
// pIn - pointer to the image data. Formatted such that
//       pixel (x,y) is at pIn + pixelSize * x + stride * y;
// pOut - pointer to encoded data. Must be large enough to store entire encoded image.
// pixelSize can be 2 or 3. 2 is an GL_UNSIGNED_SHORT_5_6_5 image, 3 is a GL_BYTE RGB image.
// returns non-zero if there is an error.

bool etc1_encode_image(const unsigned char* pIn, unsigned int width, unsigned int height,
         unsigned int pixelSize, unsigned int stride, unsigned char* pOut, unsigned int outWidth,
         unsigned int outHeight);

#endif // THIRD_PARTY_ANDROID_OPENGL_ETC1_ETC1_H_
