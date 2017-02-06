{
  'includes': [
    'extensions_tests.gypi',
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
      'target_name': 'extensions_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:cast_certificate_test_support',
        '../components/components.gyp:keyed_service_content',
        '../components/components.gyp:pref_registry_test_support',
        '../components/components.gyp:user_prefs',
        '../content/content_shell_and_tests.gyp:test_support_content',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '../device/core/core.gyp:device_core',
        '../device/hid/hid.gyp:device_hid',
        '../device/serial/serial.gyp:device_serial',
        '../device/serial/serial.gyp:device_serial_test_util',
        '../mojo/mojo_edk.gyp:mojo_js_lib',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../services/shell/shell_public.gyp:shell_public',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        'common/api/api.gyp:cast_channel_proto',
        'extensions.gyp:extensions_browser',
        'extensions.gyp:extensions_common',
        'extensions.gyp:extensions_renderer',
        'extensions.gyp:extensions_shell_and_test_pak',
        'extensions.gyp:extensions_test_support',
        'extensions.gyp:extensions_utility',
        'extensions_resources.gyp:extensions_resources',
        'extensions_strings.gyp:extensions_strings',
      ],
      # Needed for third_party libraries like leveldb.
      'include_dirs': [
        '..',
      ],
      'sources': [
        '<@(extensions_unittests_sources)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos_test_support',
          ],
          'sources': [
            'browser/api/webcam_private/visca_webcam_unittest.cc',
          ],
        }],
        ['proprietary_codecs==1 and enable_wifi_display==1', {
          'sources': [
            '<@(extensions_unittests_sources_wifi_display)',
          ],
        }],
      ],
    },
    {
      # GN version: //extensions:extensions_browsertests
      'target_name': 'extensions_browsertests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'extensions.gyp:extensions_test_support',
        'shell/app_shell.gyp:app_shell_lib',
        # TODO(yoz): find the right deps
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/components/components.gyp:guest_view_browser',
        '<(DEPTH)/components/components.gyp:guest_view_renderer',
        '<(DEPTH)/components/components.gyp:guest_view_test_support',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_browser_test_base',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_browser_test_support',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '<(DEPTH)/device/core/core.gyp:device_core_mocks',
        '<(DEPTH)/device/hid/hid.gyp:device_hid_mocks',
        '<(DEPTH)/device/usb/usb.gyp:device_usb_mocks',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(extensions_browsertests_sources)',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            'shell/app_shell.gyp:app_shell',  # Needed for App Shell.app's Helper.
          ],
        }],
        # This is only here to keep gyp happy. This target never builds on
        # mobile platforms.
        ['OS != "ios" and OS != "android"', {
          'dependencies': [
            '<(DEPTH)/components/components.gyp:storage_monitor_test_support',
          ],
        }],
      ]
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'extensions_browsertests_run',
          'type': 'none',
          'dependencies': [
            'extensions_browsertests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'extensions_browsertests.isolate',
          ],
        },
        {
          'target_name': 'extensions_unittests_run',
          'type': 'none',
          'dependencies': [
            'extensions_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'extensions_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        }
      ],
    }],
  ],
}
