# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/base:unittests
      'target_name': 'ui_base_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../../url/url.gyp:url_lib',
        '../display/display.gyp:display',
        '../events/events.gyp:events_base',
        '../gfx/gfx.gyp:gfx_test_support',
        '../resources/ui_resources.gyp:ui_resources',
        '../resources/ui_resources.gyp:ui_test_pak',
        '../strings/ui_strings.gyp:ui_strings',
        'ime/ui_base_ime.gyp:ui_base_ime',
        'ui_base.gyp:ui_base',
        'ui_base.gyp:ui_data_pack',
        'ui_base.gyp:ui_base_test_support',
      ],
      # iOS uses a small subset of ui. common_sources are the only files that
      # are built on iOS.
      'common_sources' : [
        # Note: file list duplicated in GN build.
        'clipboard/clipboard_util_mac_unittest.mm',
        'l10n/l10n_util_mac_unittest.mm',
        'l10n/l10n_util_unittest.cc',
        'l10n/l10n_util_win_unittest.cc',
        'l10n/time_format_unittest.cc',
        'layout_unittest.cc',
        'models/tree_node_iterator_unittest.cc',
        'resource/data_pack_literal.cc',
        'resource/data_pack_unittest.cc',
        'resource/resource_bundle_mac_unittest.mm',
        'resource/resource_bundle_unittest.cc',
        'resource/scale_factor_unittest.cc',
        'template_expressions_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      'all_sources': [
        # Note: file list duplicated in GN build.
        '<@(_common_sources)',
        'accelerators/accelerator_manager_unittest.cc',
        'accelerators/menu_label_accelerator_util_linux_unittest.cc',
        'clipboard/custom_data_helper_unittest.cc',
        'cocoa/base_view_unittest.mm',
        'cocoa/cocoa_base_utils_unittest.mm',
        'cocoa/constrained_window/constrained_window_animation_unittest.mm',
        'cocoa/controls/blue_label_button_unittest.mm',
        'cocoa/controls/hover_image_menu_button_unittest.mm',
        'cocoa/controls/hyperlink_button_cell_unittest.mm',
        'cocoa/controls/hyperlink_text_view_unittest.mm',
        'cocoa/focus_tracker_unittest.mm',
        'cocoa/fullscreen_window_manager_unittest.mm',
        'cocoa/hover_image_button_unittest.mm',
        'cocoa/menu_controller_unittest.mm',
        'cocoa/nscolor_additions_unittest.mm',
        'cocoa/nsgraphics_context_additions_unittest.mm',
        'cocoa/nsview_additions_unittest.mm',
        'cocoa/three_part_image_unittest.mm',
        'cocoa/tracking_area_unittest.mm',
        'dragdrop/os_exchange_data_provider_aurax11_unittest.cc',
        'ime/candidate_window_unittest.cc',
        'ime/chromeos/character_composer_unittest.cc',
        'ime/composition_text_unittest.cc',
        'ime/composition_text_util_pango_unittest.cc',
        'ime/input_method_base_unittest.cc',
        'ime/input_method_chromeos_unittest.cc',
        'ime/win/imm32_manager_unittest.cc',
        'ime/win/tsf_input_scope_unittest.cc',
        'material_design/material_design_controller_unittest.cc',
        'models/list_model_unittest.cc',
        'models/list_selection_model_unittest.cc',
        'models/tree_node_model_unittest.cc',
        'test/data/resource.h',
        'test/scoped_fake_nswindow_fullscreen_unittest.mm',
        'test/test_clipboard_unittest.cc',
        'text/bytes_formatting_unittest.cc',
        'user_activity/user_activity_detector_unittest.cc',
        'view_prop_unittest.cc',
        'webui/web_ui_util_unittest.cc',
        'x/selection_requestor_unittest.cc',
      ],
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS!="ios"', {
          'sources' : [ '<@(_all_sources)' ],
        }, {  # OS=="ios"
          'sources' : [ '<@(_common_sources)' ],
          # The ResourceBundle unittest expects a locale.pak file to exist in
          # the bundle for English-US. Copy it in from where it was generated
          # by ui_resources.gyp:ui_test_pak.
          'mac_bundle_resources': [
            '<(PRODUCT_DIR)/ui/en.lproj/locale.pak',
          ],
          'actions': [
            {
              'action_name': 'copy_test_data',
              'variables': {
                'test_data_files': [
                  'test/data',
                ],
                'test_data_prefix' : 'ui/base',
              },
              'includes': [ '../../build/copy_test_data_ios.gypi' ],
            },
          ],
        }],
        ['OS == "linux" and use_aura==1 and chromeos==0', {
          'sources': [
            'ime/input_method_auralinux_unittest.cc',
          ]
        }],
        ['OS == "win"', {
          'sources': [
            'dragdrop/os_exchange_data_win_unittest.cc',
            'win/hwnd_subclass_unittest.cc',
            'win/open_file_name_win_unittest.cc',
            'win/osk_display_manager_unittest.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'd2d1.dll',
                'd3d10_1.dll',
              ],
              'AdditionalDependencies': [
                'd2d1.lib',
                'd3d10_1.lib',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-limm32.lib',
              '-loleacc.lib',
            ],
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
          'sources!': [
            'user_activity/user_activity_detector_unittest.cc',
          ],
        }],
        ['use_pango == 1', {
          'dependencies': [
            '../../build/linux/system.gyp:pangocairo',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
            '../events/platform/x11/x11_events_platform.gyp:x11_events_platform',
            '../gfx/x/gfx_x11.gyp:gfx_x11',
            'x/ui_base_x.gyp:ui_base_x',
          ],
        }],
        ['OS!="win" or use_aura==0', {
          'sources!': [
            'view_prop_unittest.cc',
          ],
        }],
        ['use_x11==1 and use_aura==1',  {
          'sources': [
            'cursor/cursor_loader_x11_unittest.cc',
          ],
        }],
        ['OS=="mac"',  {
          'dependencies': [
            '../../third_party/mozilla/mozilla.gyp:mozilla',
            '../events/events.gyp:events_test_support',
            'ui_base_tests_bundle',
          ],
          'conditions': [
            ['component=="static_library"', {
              # Needed for mozilla.gyp.
              'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
            }],
          ],
        }],
        ['use_aura==1 or toolkit_views==1',  {
          'sources': [
            'dragdrop/os_exchange_data_unittest.cc',
          ],
          'dependencies': [
            '../events/events.gyp:events',
            '../events/events.gyp:events_base',
            '../events/events.gyp:events_test_support',
            '../events/platform/events_platform.gyp:events_platform',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
          ],
          'sources!': [
            'dragdrop/os_exchange_data_provider_aurax11_unittest.cc',
            'x/selection_requestor_unittest.cc',
          ],
        }],
        ['use_x11==0', {
          'sources!': [
            'ime/composition_text_util_pango_unittest.cc',
            'ime/input_method_chromeos_unittest.cc',
          ],
        }],
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^l10n/l10n_util_mac_unittest\\.mm$'],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # Mac target to build a test Framework bundle to mock out resource loading.
    ['OS == "mac"', {
      'targets': [
        {
          'target_name': 'ui_base_tests_bundle',
          'type': 'shared_library',
          'dependencies': [
            '../resources/ui_resources.gyp:ui_test_pak',
          ],
          'includes': [ 'ui_base_tests_bundle.gypi' ],
          # ui_base_tests_bundle doesn't actually contain a shared library and
          # therefore should not depend on sanitizer_options or any other
          # libraries. Adding such a dependency will result in creating a
          # broken shared library within the bundle.
          'conditions': [
            ['use_sanitizer_options==1', {
              'dependencies!': [
                '../../build/sanitizers/sanitizers.gyp:sanitizer_options',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'ui_base_unittests_apk',
          'type': 'none',
          'dependencies': [
            # TODO(tfarina): This is a layer violation and should be removed.
            # crbug.com/176960
            # For now this is here as a temporary band-aid to fix the clobber
            # issue we are seeing when running this target on Android.
            # crbug.com/374490
            '../../chrome/chrome_resources.gyp:packed_resources',
            'ui_base_unittests',
          ],
          'variables': {
            'test_suite_name': 'ui_base_unittests',
            'isolate_file': 'ui_base_tests.isolate',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"',
          {
            'targets': [
              {
                'target_name': 'ui_base_unittests_apk_run',
                'type': 'none',
                'dependencies': [
                  'ui_base_unittests_apk',
                ],
                'includes': [
                  '../../build/isolate.gypi',
                ],
                'sources': [
                  'ui_base_unittests_apk.isolate',
                ],
              },
            ],
          },
        ],
      ],
    }],
    ['test_isolation_mode != "noop" and OS != "android"', {
      'targets': [
        {
          'target_name': 'ui_base_unittests_run',
          'type': 'none',
          'dependencies': [
            'ui_base_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'ui_base_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
