# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/upload_list
      'target_name': 'upload_list',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'upload_list/crash_upload_list.cc',
        'upload_list/crash_upload_list.h',
        'upload_list/upload_list.cc',
        'upload_list/upload_list.h',
      ],
    },
  ],
}
