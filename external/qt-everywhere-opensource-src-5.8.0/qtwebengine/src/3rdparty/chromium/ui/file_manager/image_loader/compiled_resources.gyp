# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          '../../webui/resources/js/assert.js',
          '../file_manager/common/js/file_type.js',
          '../file_manager/common/js/metrics_base.js',
          '../file_manager/common/js/metrics.js',
          '../file_manager/common/js/metrics_events.js',
          '../file_manager/foreground/js/metadata/image_orientation.js',
          'cache.js',
          'image_loader.js',
          'piex_loader.js',
          'request.js',
          'scheduler.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '<(EXTERNS_DIR)/file_manager_private.js',
          '<(EXTERNS_DIR)/metrics_private.js',
          '../../../third_party/analytics/externs.js',
        ],
      },
      'includes': [
        '../compile_js.gypi',
      ],
    }
  ],
}
