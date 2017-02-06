# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'data_use_measurement_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
      ],
      'sources': [
        'data_use_measurement/core/data_use_user_data.cc',
        'data_use_measurement/core/data_use_user_data.h',
      ]
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'data_use_measurement_content',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../net/net.gyp:net',
            'data_use_measurement_core',
          ],
          'sources': [
            'data_use_measurement/content/data_use_measurement.cc',
            'data_use_measurement/content/data_use_measurement.h',
          ]
        },
      ],
    }],
  ],
}
