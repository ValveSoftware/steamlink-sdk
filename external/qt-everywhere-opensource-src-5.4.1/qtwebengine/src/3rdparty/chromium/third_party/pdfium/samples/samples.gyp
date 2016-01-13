# Copyright 2014 PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'type': 'executable',
    'dependencies': [
      '../pdfium.gyp:pdfium',
    ],
    'include_dirs': ['<(DEPTH)'],
  },
  'targets': [
    {
      'target_name': 'pdfium_test',
      'sources': [
        'pdfium_test.cc',
      ],
    },
  ],
}
