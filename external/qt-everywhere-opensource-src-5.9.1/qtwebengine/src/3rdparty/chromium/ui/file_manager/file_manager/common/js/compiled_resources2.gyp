# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
#    {
#      'target_name': 'async_util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'async_util_unittest',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'error_util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'externs',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_type',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'importer_common',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'importer_common_unittest',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'lru_cache',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'lru_cache_unittest',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metrics',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metrics_base',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metrics_events',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metrics_unittest',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_entry',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'progress_center_common',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_importer_common',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_tracker',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'unittest_util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'volume_manager_common',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '../../../externs/compiled_resources2.gyp:volume_info',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
  ],
}
