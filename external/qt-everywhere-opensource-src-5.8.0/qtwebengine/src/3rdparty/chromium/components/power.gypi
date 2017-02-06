# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'power',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'keyed_service_content',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'power/origin_power_map.cc',
        'power/origin_power_map.h',
        'power/origin_power_map_factory.cc',
        'power/origin_power_map_factory.h',
      ],
    },
  ],
}
