# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ppapi/lib/gl/gles2
      'target_name': 'ppapi_gles2',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'lib/gl/gles2/gl2ext_ppapi.c',
        'lib/gl/gles2/gl2ext_ppapi.h',
        'lib/gl/gles2/gles2.c',
      ],
    },
  ],
}
