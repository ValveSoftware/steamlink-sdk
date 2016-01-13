# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'shell_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'non_compiled_schema_files': [
        ],
        # TODO: Eliminate these on Android. See crbug.com/305852.
        'schema_files': [
          'shell.idl',
        ],
        'cc_dir': 'apps/shell/common/api',
        'root_namespace': 'apps::shell_api',
        'impl_dir': 'apps/shell/browser/api',
      },
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
    },
  ],
}
