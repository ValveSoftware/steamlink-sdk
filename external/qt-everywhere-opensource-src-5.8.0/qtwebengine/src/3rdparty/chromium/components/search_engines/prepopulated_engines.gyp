# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/search_engines:prepopulated_engines
      'target_name': 'prepopulated_engines',
      'type': 'static_library',
      'sources': [
        'prepopulated_engines.json',
      ],
      'includes': [
        '../../build/json_to_struct.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'schema_file': 'prepopulated_engines_schema.json',
        'namespace': 'TemplateURLPrepopulateData',
        'cc_dir': 'components/search_engines',
      },
    },
  ],
}
