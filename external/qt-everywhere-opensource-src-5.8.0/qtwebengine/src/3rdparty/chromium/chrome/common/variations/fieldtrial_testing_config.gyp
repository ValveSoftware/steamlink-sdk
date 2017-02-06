# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome/common/variations:fieldtrial_testing_config
      'target_name': 'fieldtrial_testing_config',
      'type': 'static_library',
      'sources': [
        '../../../testing/variations/fieldtrial_testing_config_android.json',
        '../../../testing/variations/fieldtrial_testing_config_win.json',
        '../../../testing/variations/fieldtrial_testing_config_linux.json',
        '../../../testing/variations/fieldtrial_testing_config_mac.json',
        '../../../testing/variations/fieldtrial_testing_config_chromeos.json',
        '../../../testing/variations/fieldtrial_testing_config_ios.json',
      ],
      'conditions': [
        ['OS!="android"', {'sources/': [['exclude', '_android\\.json$']]}],
        ['OS!="win"', {'sources/': [['exclude', '_win\\.json$']]}],
        ['OS!="linux" or chromeos==1', {'sources/': [['exclude', '_linux\\.json$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac\\.json$']]}],
        ['chromeos!=1', {'sources/': [['exclude', '_chromeos\\.json$']]}],
        ['OS!="ios"', {'sources/': [['exclude', '_ios\\.json$']]}],
      ],
      'includes': [
        '../../../build/json_to_struct.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'schema_file': 'fieldtrial_testing_config_schema.json',
        'namespace': 'chrome_variations',
        'cc_dir': 'chrome/common/variations',
        'struct_gen': '<(DEPTH)/tools/variations/fieldtrial_to_struct.py',
        'output_filename': 'fieldtrial_testing_config',
      },
    },
  ],
}
