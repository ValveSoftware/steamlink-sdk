# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# File included in remoting_webapp_* targets in remoting_client.gypi

{
  'type': 'none',
  'variables': {
    'extra_files%': [],
    'main_html_file%': '<(SHARED_INTERMEDIATE_DIR)/remoting/main.html',
    'generated_html_files': [
      '<(SHARED_INTERMEDIATE_DIR)/remoting/background.html',
      '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
      '<(SHARED_INTERMEDIATE_DIR)/remoting/message_window.html',
      '<(SHARED_INTERMEDIATE_DIR)/remoting/wcs_sandbox.html',
      '<(SHARED_INTERMEDIATE_DIR)/remoting/public_session.html',
    ],
    'dr_webapp_locales_listfile': '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)_locales.txt',
  },
  'dependencies': [
    'remoting_resources',
    'remoting_webapp_html',
  ],
  'conditions': [
    ['run_jscompile != 0', {
      'dependencies': ['remoting_webapp_compile.gypi:*'],
    }],
  ],
  'actions': [
    {
      'action_name': 'Build Remoting locales listfile',
      'inputs': [
        '<(remoting_localize_path)',
      ],
      'outputs': [
        '<(dr_webapp_locales_listfile)',
      ],
      'action': [
        'python', '<(remoting_localize_path)',
        '--locale_output',
        '"<(webapp_locale_dir)/@{json_suffix}/messages.json"',
        '--locales_listfile',
        '<(dr_webapp_locales_listfile)',
        '<@(remoting_locales)',
      ],
    },
    {
      'action_name': 'Build Remoting WebApp',
      'inputs': [
        'webapp/build-webapp.py',
        'webapp/crd/manifest.json.jinja2',
        '<(chrome_version_path)',
        '<(remoting_version_path)',
        '<(dr_webapp_locales_listfile)',
        '<@(generated_html_files)',
        '<(main_html_file)',
        '<@(remoting_webapp_crd_files)',
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
        'webapp/crd/manifest.json.jinja2',
        '<@(generated_html_files)',
        '<(main_html_file)',
        '<@(remoting_webapp_crd_files)',
        '<@(extra_files)',
        '--locales_listfile',
        '<(dr_webapp_locales_listfile)',
        '--use_gcd',
        '<(remoting_use_gcd)',
      ],
    },
  ],
}
