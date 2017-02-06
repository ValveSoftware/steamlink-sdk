# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //remoting/client
      'target_name': 'remoting_client',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'VERSION=<(version_full)',
      ],
      'dependencies': [
        'remoting_base',
        'remoting_protocol',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
      ],
      'sources': [
        '<@(remoting_client_sources)',
        '<@(remoting_client_standalone_sources)',
      ],
      'conditions': [
        ['buildtype!="Official"', {
          'defines': [
            'ENABLE_WEBRTC_REMOTING_CLIENT'
          ]
        }]
      ],
    },  # end of target 'remoting_client'

    {
      # GN version: See remoting/webapp/build_template.gni
      'target_name': 'remoting_webapp_html',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Build Remoting Webapp main.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_main)',
            '<@(remoting_webapp_template_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/main.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/main.html',
            '<(remoting_webapp_template_main)',
            '--template-dir', '<(DEPTH)/remoting',
            '--templates', '<@(remoting_webapp_template_files)',
            '--js',
            '<@(remoting_webapp_crd_main_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp wcs_sandbox.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_wcs_sandbox)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/wcs_sandbox.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/wcs_sandbox.html',
            '<(remoting_webapp_template_wcs_sandbox)',
            '--js', '<@(remoting_webapp_wcs_sandbox_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp background.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_background)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/background.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/background.html',
            '<(remoting_webapp_template_background)',
            '--js', '<@(remoting_webapp_background_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp message_window.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_message_window)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/message_window.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/message_window.html',
            '<(remoting_webapp_template_message_window)',
            '--js', '<@(remoting_webapp_message_window_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp public_session.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_public_session)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/public_session.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/public_session.html',
            '<(remoting_webapp_template_public_session)',
            '--template-dir', '<(DEPTH)/remoting',
            '--templates', '<@(remoting_webapp_public_session_template_files)',
            '--js',
            '<@(remoting_webapp_public_session_html_all_js_files)',
          ],
        },
      ],  # end of actions
    },  # end of target 'remoting_webapp_html'

    {
      # GN version: //remoting/webapp:credits
      'target_name': 'remoting_client_credits',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Build remoting client credits',
          'inputs': [
            '../tools/licenses.py',
            'webapp/base/html/credits.tmpl',
            'webapp/base/html/credits_entry.tmpl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
          ],
          'hard_dependency': 1,
          'action': ['python',
                     '../tools/licenses.py',
                     'credits',
                     '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
                     '--file-template', 'webapp/base/html/credits.tmpl',
                     '--entry-template', 'webapp/base/html/credits_entry.tmpl',

          ],
        },
      ],
    },
    {
      # GN version: //remoting/webapp
      'target_name': 'remoting_webapp',
      'type': 'none',
      'variables': {
        'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp.v2',
        'zip_path': '<(PRODUCT_DIR)/remoting-webapp.v2.zip',
        'extra_files': [
          'webapp/crd/remoting_client_pnacl.nmf.jinja2',
        ],
      },
      'conditions': [
        ['disable_nacl==0 and disable_nacl_untrusted==0', {
          'variables': {
            'extra_files': [
              '<(PRODUCT_DIR)/remoting_client_plugin_newlib.pexe',
            ],
          },
          'dependencies': [
            'remoting_nacl.gyp:remoting_client_plugin_nacl',
          ],
        }],
        ['disable_nacl==0 and disable_nacl_untrusted==0 and buildtype == "Dev"', {
          'variables': {
            'extra_files': [
              '<(PRODUCT_DIR)/remoting_client_plugin_newlib.pexe.debug',
            ],
          },
        }],
      ],
      'includes': [ 'remoting_webapp.gypi', ],
    },  # end of target 'remoting_webapp'
  ],  # end of targets
}
