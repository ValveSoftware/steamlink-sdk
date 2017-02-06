# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'schema_files': [
      'identity.idl',
    ],
    'non_compiled_schema_files': [
    ],
    # Chrome OS specific APIs.
    'chromeos_schema_files': [
    ],
    
    'chromium_code': 1,
    'cc_dir': 'extensions/shell/common/api',
    'root_namespace': 'extensions::shell::api::%(namespace)s',
    'bundle_name': 'Shell',
    'impl_dir_': 'extensions/shell/browser/api',
    
    'conditions': [
      ['chromeos==1', {
        'schema_files': [
          '<@(chromeos_schema_files)',
        ],
      }],
    ],
  },
}
