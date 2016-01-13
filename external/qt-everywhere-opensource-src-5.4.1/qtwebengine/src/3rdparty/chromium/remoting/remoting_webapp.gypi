# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# File included in remoting_webapp_* targets in remoting_client.gypi

{
  'type': 'none',
  'variables': {
    'extra_files%': [],
    'generated_html_files': [
      '<(SHARED_INTERMEDIATE_DIR)/main.html',
      '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
    ],
  },
  'dependencies': [
    'remoting_resources',
    'remoting_webapp_html',
  ],
  'conditions': [
    ['run_jscompile != 0', {
      'variables': {
        'success_stamp': '<(PRODUCT_DIR)/remoting_webapp_jscompile.stamp',
      },
      'actions': [
        {
          'action_name': 'Verify remoting webapp',
          'inputs': [
            '<@(remoting_webapp_all_js_files)',
            '<@(remoting_webapp_js_proto_files)',
          ],
          'outputs': [
            '<(success_stamp)',
          ],
          'action': [
            'python', 'tools/jscompile.py',
            '<@(remoting_webapp_all_js_files)',
            '<@(remoting_webapp_js_proto_files)',
            '--success-stamp', '<(success_stamp)'
          ],
        },
      ],  # actions
    }],
  ],
  'actions': [
    {
      'action_name': 'Build Remoting WebApp',
      'inputs': [
        'webapp/build-webapp.py',
        'webapp/manifest.json.jinja2',
        '<(chrome_version_path)',
        '<(remoting_version_path)',
        '<@(generated_html_files)',
        '<@(remoting_webapp_files)',
        '<@(remoting_webapp_locale_files)',
        '<@(extra_files)',
      ],
      'outputs': [
        '<(output_dir)',
        '<(zip_path)',
      ],
      'action': [
        'python', 'webapp/build-webapp.py',
        '<(buildtype)',
        '<(version_full)',
        '<(output_dir)',
        '<(zip_path)',
        'webapp/manifest.json.jinja2',
        '<(webapp_type)',
        '<@(generated_html_files)',
        '<@(remoting_webapp_files)',
        '<@(extra_files)',
        '--locales', '<@(remoting_webapp_locale_files)',
      ],
    },
  ],
}
