# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {

    'branding_path': '../remoting/branding_<(branding)',

    'conditions': [
      ['OS=="mac"', {
        'mac_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_BUNDLE_ID@")',
        'mac_creator': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_CREATOR@")',
      }],
      ['OS=="win"', {
        # Each CLSID is a hash of the current version string salted with an
        # arbitrary GUID. This ensures that the newly installed COM classes will
        # be used during/after upgrade even if there are old instances running
        # already.
        # The IDs are not random to avoid rebuilding host when it's not
        # necessary.
        'daemon_controller_clsid':
            '<!(python -c "import uuid; print uuid.uuid5(uuid.UUID(\'<(daemon_controller_guid)\'), \'<(version_full)\')")',
        'rdp_desktop_session_clsid':
            '<!(python -c "import uuid; print uuid.uuid5(uuid.UUID(\'<(rdp_desktop_session_guid)\'), \'<(version_full)\')")',
      }],
    ],
  },

  'includes': [
    '../chrome/js_unittest_vars.gypi',
    'remoting_client.gypi',
    'remoting_host.gypi',
    'remoting_host_srcs.gypi',
    'remoting_ios.gypi',
    'remoting_key_tester.gypi',
    'remoting_locales.gypi',
    'remoting_options.gypi',
    'remoting_srcs.gypi',
    'remoting_test.gypi',
    'remoting_version.gypi',
    'remoting_webapp_files.gypi',
  ],

  'target_defaults': {
    'defines': [
      'BINARY_CORE=1',
      'BINARY_DESKTOP=2',
      'BINARY_HOST_ME2ME=3',
      'BINARY_NATIVE_MESSAGING_HOST=4',
      'BINARY_REMOTE_ASSISTANCE_HOST=5',
      'BINARY_REMOTE_SECURITY_KEY=6',
      'BINARY_REMOTING_START_HOST=7',
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
    ],
    'variables': {
      'win_debug_RuntimeChecks': '0',
    },
    'conditions': [
      ['OS=="mac" and mac_breakpad==1', {
        'defines': [
          'REMOTING_ENABLE_BREAKPAD'
        ],
      }],
      ['OS=="win" and buildtype == "Official"', {
        'defines': [
          'REMOTING_ENABLE_BREAKPAD'
        ],
      }],
      ['OS=="win" and remoting_multi_process != 0 and \
          remoting_rdp_session != 0', {
        'defines': [
          'REMOTING_RDP_SESSION',
        ],
      }],
      ['remoting_multi_process != 0', {
        'defines': [
          'REMOTING_MULTI_PROCESS',
        ],
      }],
    ],
  },

  'targets': [
    {
      # GN version: //remoting/base:breakpad
      'target_name': 'remoting_breakpad',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'base/breakpad.h',
        'base/breakpad_linux.cc',
        'base/breakpad_mac.mm',
        'base/breakpad_win.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
          ],
        }],
      ],
    },  # end of target 'remoting_breakpad'

    {
      # GN version: //remoting/resources
      'target_name': 'remoting_resources',
      'type': 'none',
      'dependencies': [
        'remoting_webapp_html',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
        'sources': [
          'host/continue_window_mac.mm',
          'host/disconnect_window_mac.mm',
          'host/installer/mac/uninstaller/remoting_uninstaller-InfoPlist.strings.jinja2',
          'host/it2me/it2me_confirmation_dialog_chromeos.cc',
          'host/mac/me2me_preference_pane-InfoPlist.strings.jinja2',
          'host/resources_unittest.cc',
          'host/win/core.rc.jinja2',
          'host/win/host_messages.mc.jinja2',
          'host/win/version.rc.jinja2',
          'resources/play_store_resources.cc',
          '<@(desktop_remoting_webapp_localizable_files )',
        ],
      },
      'actions': [
        {
          # GN version: //remoting/resources:verify_resources
          'action_name': 'verify_resources',
          'inputs': [
            'resources/remoting_strings.grd',
            'tools/verify_resources.py',
            '<@(sources)'
          ],
          'outputs': [
            '<(PRODUCT_DIR)/remoting_resources_verified.stamp',
          ],
          'action': [
            'python',
            'tools/verify_resources.py',
            '-t', '<(PRODUCT_DIR)/remoting_resources_verified.stamp',
            '-r', 'resources/remoting_strings.grd',
            '<@(sources)',
         ],
        },
        {
          # GN version: //remoting/resources:strings
          'action_name': 'remoting_strings',
          'variables': {
            'grit_grd_file': 'resources/remoting_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          # GN version: //remoting/resources:copy_locales
          'action_name': 'copy_locales',
          'variables': {
            'copy_output_dir%': '<(PRODUCT_DIR)',
          },
          'inputs': [
            'tools/build/remoting_copy_locales.py',
            '<!@pymod_do_main(remoting_copy_locales -i -p <(OS) -g <(grit_out_dir) <(remoting_locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(copy_output_dir) <(remoting_locales))'
          ],
          'action': [
            'python', 'tools/build/remoting_copy_locales.py',
            '-p', '<(OS)',
            '-g', '<(grit_out_dir)',
            '-x', '<(copy_output_dir)/.',
            '<@(remoting_locales)',
          ],
        }
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },  # end of target 'remoting_resources'

    {
      # GN version: //remoting/base and //remoting/codec
      'target_name': 'remoting_base',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
        '../net/net.gyp:net',
        '../third_party/libvpx/libvpx.gyp:libvpx',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/opus/opus.gyp:opus',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
        'remoting_resources',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      # This target needs a hard dependency because dependent targets
      # depend on chromotocol_proto_lib for headers.
      'hard_dependency': 1,
      'sources': [
        '<@(remoting_base_sources)',
        '<@(remoting_codec_sources)',
      ],
    },  # end of target 'remoting_base'

    {
      # GN version: //remoting/protocol
      'target_name': 'remoting_protocol',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../jingle/jingle.gyp:jingle_glue',
        '../net/net.gyp:net',
        '../third_party/boringssl/boringssl.gyp:boringssl',
        '../third_party/expat/expat.gyp:expat',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        'remoting_base',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'sources': [
        '<@(remoting_protocol_sources)',
        '<@(remoting_protocol_nonnacl_sources)',
        '<@(remoting_signaling_sources)',
      ],
      'conditions': [
        ['enable_webrtc == 1', {
          'dependencies': [
            '../third_party/libjingle/libjingle.gyp:libjingle_webrtc',
          ],
        }],
      ],
    },  # end of target 'remoting_protocol'
  ],  # end of targets
}
