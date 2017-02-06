# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'app_shell.gypi',
  ],
  'variables': {
    # Product name is used for Mac bundle.
    'app_shell_product_name': 'App Shell',
    # The version is high enough to be supported by Omaha (at least 31)
    # but fake enough to be obviously not a Chrome release.
    'app_shell_version': '38.1234.5678.9',
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //extensions/shell:app_shell_lib
      'target_name': 'app_shell_lib',
      'type': 'static_library',
      'dependencies': [
        'app_shell_version_header',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/components/components.gyp:devtools_discovery',
        '<(DEPTH)/components/components.gyp:devtools_http_handler',
        '<(DEPTH)/components/components.gyp:pref_registry',
        '<(DEPTH)/components/components.gyp:update_client',
        '<(DEPTH)/components/components.gyp:user_prefs',
        '<(DEPTH)/components/components.gyp:web_cache_renderer',
        '<(DEPTH)/components/prefs/prefs.gyp:prefs',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_lib',
        '<(DEPTH)/device/core/core.gyp:device_core',
        '<(DEPTH)/device/hid/hid.gyp:device_hid',
        '<(DEPTH)/extensions/browser/api/api_registration.gyp:extensions_api_registration',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_common',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions.gyp:extensions_utility',
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
        '<(DEPTH)/extensions/shell/browser/api/api_registration.gyp:shell_api_registration',
        '<(DEPTH)/extensions/shell/common/api/api.gyp:shell_api',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
        '<(DEPTH)/ui/base/ime/ui_base_ime.gyp:ui_base_ime',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/v8/src/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/content/content.gyp:content_browser',
      ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/shell',
      ],
      'sources': [
        '<@(app_shell_lib_sources)',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '<(DEPTH)/ui/wm/wm.gyp:wm',
          ],
          'sources': [
            '<@(app_shell_lib_sources_aura)',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos',
            '<(DEPTH)/ui/display/display.gyp:display',
          ],
          'sources': [
            '<@(app_shell_lib_sources_chromeos)',
          ],
        }],
        ['disable_nacl==0 and OS=="linux"', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl_helper',
          ],
        }],
        ['disable_nacl==0', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl',
            '<(DEPTH)/components/nacl.gyp:nacl_browser',
            '<(DEPTH)/components/nacl.gyp:nacl_common',
            '<(DEPTH)/components/nacl.gyp:nacl_renderer',
            '<(DEPTH)/components/nacl.gyp:nacl_switches',
          ],
          'sources': [
            '<@(app_shell_lib_sources_nacl)',
          ],
        }],
      ],
    },
    {
      # GN version: //extensions/shell:app_shell
      'target_name': 'app_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<@(app_shell_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
            },
          },
          'dependencies': [
            '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['OS=="mac"', {
          'product_name': '<(app_shell_product_name)',
          'dependencies!': [
            'app_shell_lib',
          ],
          'dependencies': [
            'app_shell_framework',
            'app_shell_helper',
          ],
          'mac_bundle_resources': [
            'app/app-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list app-Info.plist once, not the three times it is
          # listed here.
          'mac_bundle_resources!': [
            'app/app-Info.plist',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'app/app-Info.plist',
          },
          'copies': [{
              'destination': '<(PRODUCT_DIR)/<(app_shell_product_name).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(app_shell_product_name) Helper.app',
              ],
          }],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(app_shell_product_name) Framework.framework',
              'action': [
                '../../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(app_shell_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                         '--scm=1',
                         '--version=<(app_shell_version)'],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell_unittests',
      'type': 'executable',
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions.gyp:extensions_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        '<@(app_shell_unittests_sources)',
      ],
      'conditions': [
        ['disable_nacl==0', {
          'sources': [
            '<@(app_shell_unittests_sources_nacl)',
          ],
        }],
        ['use_aura==1', {
          'sources': [
            '<@(app_shell_unittests_sources_aura)',
          ],
          'dependencies': [
            '<(DEPTH)/ui/aura/aura.gyp:aura_test_support',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
          ],
          'sources': [
            '<@(app_shell_unittests_sources_chromeos)',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'variables': {
            'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
          },
          'inputs': [
            '<(version_path)',
            '<(lastchange_path)',
            'common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/extensions/shell/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(lastchange_path)',
            '-f', '<(version_path)',
            'common/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../../build/util/version.gypi',
          ],
        },
      ],
    },
  ],  # targets

  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          # GN version: //extensions/shell:app_shell_framework
          'target_name': 'app_shell_framework',
          'type': 'shared_library',
          'product_name': '<(app_shell_product_name) Framework',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(PRODUCT_DIR)/extensions_shell_and_test.pak',
            'app/framework-Info.plist',
          ],
          'mac_bundle_resources!': [
            'app/framework-Info.plist',
          ],
          'xcode_settings': {
            # The framework is placed within the .app's Framework
            # directory.  DYLIB_INSTALL_NAME_BASE and
            # LD_DYLIB_INSTALL_NAME affect -install_name.
            'DYLIB_INSTALL_NAME_BASE':
                '@executable_path/../Frameworks',
            # See /build/mac/copy_framework_unversioned.sh for
            # information on LD_DYLIB_INSTALL_NAME.
            'LD_DYLIB_INSTALL_NAME':
                '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',

            'INFOPLIST_FILE': 'app/framework-Info.plist',
          },
          'dependencies': [
            'app_shell_lib',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            '<@(app_shell_sources_mac)',
          ],
          'postbuilds': [
            {
              # Modify the Info.plist as needed.  The script explains why
              # this is needed.  This is also done in the chrome target.
              # The framework needs the Breakpad keys if this feature is
              # enabled.  It does not need the Keystone keys; these always
              # come from the outer application bundle.  The framework
              # doesn't currently use the SCM keys for anything,
              # but this seems like a really good place to store them.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                         '--breakpad=1',
                         '--keystone=0',
                         '--scm=1',
                         '--version=<(app_shell_version)',
                         '--branding=<(app_shell_product_name)'],
            },
          ],
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
            ['v8_use_external_startup_data==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        },  # target app_shell_framework
        {
          'target_name': 'app_shell_helper',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(app_shell_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'app_shell_framework',
          ],
          'sources': [
            'app/shell_main.cc',
            'app/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'app/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'app/helper-Info.plist',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Frameworks/<(app_shell_product_name) Framework.framework/<(app_shell_product_name) Framework',
                '@executable_path/../../../<(app_shell_product_name) Framework.framework/<(app_shell_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0',
                         '--version=<(app_shell_version)'],
            },
          ],
        },  # target app_shell_helper
      ],
    }],  # OS=="mac"
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'app_shell_unittests_run',
          'type': 'none',
          'dependencies': [
            'app_shell_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'app_shell_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
