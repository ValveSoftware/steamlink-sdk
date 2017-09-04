// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

namespace ui {

extern const char kSamplePakContents[] = {
    0x04, 0x00, 0x00, 0x00,               // header(version
    0x04, 0x00, 0x00, 0x00,               //        no. entries
    0x01,                                 //        encoding)
    0x01, 0x00, 0x27, 0x00, 0x00, 0x00,   // index entry 1
    0x04, 0x00, 0x27, 0x00, 0x00, 0x00,   // index entry 4
    0x06, 0x00, 0x33, 0x00, 0x00, 0x00,   // index entry 6
    0x0a, 0x00, 0x3f, 0x00, 0x00, 0x00,   // index entry 10
    0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,   // extra entry for the size of last
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4',
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '6'
};

extern const size_t kSamplePakSize = sizeof(kSamplePakContents);

extern const char kSampleCorruptPakContents[] = {
    0x04, 0x00, 0x00, 0x00,               // header(version
    0x04, 0x00, 0x00, 0x00,               //        no. entries
    0x01,                                 //        encoding)
    0x01, 0x00, 0x27, 0x00, 0x00, 0x00,   // index entry 1
    0x04, 0x00, 0x27, 0x00, 0x00, 0x00,   // index entry 4
    0x06, 0x00, 0x33, 0x00, 0x00, 0x00,   // index entry 6
    0x0a, 0x00, 0x3f, 0x00, 0x00, 0x00,   // index entry 10
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00,   // extra entry for the size of last,
                                          // extends past END OF FILE.
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4',
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '6'
};

extern const size_t kSampleCorruptPakSize = sizeof(kSampleCorruptPakContents);

extern const char kSamplePakContents2x[] = {
    0x04, 0x00, 0x00, 0x00,               // header(version
    0x01, 0x00, 0x00, 0x00,               //        no. entries
    0x01,                                 //        encoding)
    0x04, 0x00, 0x15, 0x00, 0x00, 0x00,   // index entry 4
    0x00, 0x00, 0x24, 0x00, 0x00, 0x00,   // extra entry for the size of last
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4', ' ', '2', 'x'
};

extern const size_t kSamplePakSize2x = sizeof(kSamplePakContents2x);

extern const char kEmptyPakContents[] = {
    0x04, 0x00, 0x00, 0x00,               // header(version
    0x00, 0x00, 0x00, 0x00,               //        no. entries
    0x01,                                 //        encoding)
    0x00, 0x00, 0x0f, 0x00, 0x00, 0x00    // extra entry for the size of last
};

extern const size_t kEmptyPakSize = sizeof(kEmptyPakContents);

}  // namespace ui
