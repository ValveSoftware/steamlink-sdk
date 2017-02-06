# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'accessibility',
      'type': '<(component)',
      'export_dependent_settings': [
        'ax_gen',
      ],
      'hard_dependency': 1,
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'ax_gen',
      ],
      'defines': [
        'ACCESSIBILITY_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under accessibility, except unittests
        'ax_node.cc',
        'ax_node.h',
        'ax_node_data.cc',
        'ax_node_data.h',
        'ax_serializable_tree.cc',
        'ax_serializable_tree.h',
        'ax_text_utils.cc',
        'ax_text_utils.h',
        'ax_tree.cc',
        'ax_tree.h',
        'ax_tree_combiner.cc',
        'ax_tree_combiner.h',
        'ax_tree_data.cc',
        'ax_tree_data.h',
        'ax_tree_serializer.cc',
        'ax_tree_serializer.h',
        'ax_tree_source.h',
        'ax_tree_update.h',
        'ax_view_state.cc',
        'ax_view_state.h',
        'platform/atk_util_auralinux.cc',
        'platform/atk_util_auralinux.h',
        'platform/ax_platform_node.cc',
        'platform/ax_platform_node.h',
        'platform/ax_platform_node_auralinux.cc',
        'platform/ax_platform_node_auralinux.h',
        'platform/ax_platform_node_base.cc',
        'platform/ax_platform_node_base.h',
        'platform/ax_platform_node_delegate.h',
        'platform/ax_platform_node_mac.h',
        'platform/ax_platform_node_mac.mm',
      ],
      'conditions': [
        ['OS=="win" and use_qt==0', {
          'dependencies': [
            '../../third_party/iaccessible2/iaccessible2.gyp:iaccessible2'
          ],
          'sources': [
            'platform/ax_platform_node_win.h',
            'platform/ax_platform_node_win.cc',
          ]
        }],
        ['OS=="linux" and chromeos==0 and use_x11==1 and use_qt==0', {
          'dependencies': [
            '../../build/linux/system.gyp:atk',
            '../../build/linux/system.gyp:gconf',
            '../../build/linux/system.gyp:glib',
          ],
          'variables': {
            'clang_warning_flags': [
              # glib uses the pre-c++11 typedef-as-static_assert hack.
              '-Wno-unused-local-typedefs',
              # G_DEFINE_TYPE automatically generates a
              # *get_instance_private inline function after glib 2.37.
              # That's unused. Prevent to complain about it.
              '-Wno-unused-function',
            ],
          },
        }],
        ['OS!="linux" or chromeos==1 or use_x11==0 or use_qt==1', {
          'sources!': [
            'platform/ax_platform_node_auralinux.cc',
            'platform/ax_platform_node_auralinux.h',
            'platform/atk_util_auralinux.cc',
            'platform/atk_util_auralinux.h',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'ui_accessibility_java'
          ],
        }],
      ],
    },
    {
      'target_name': 'accessibility_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        'accessibility'
      ],
      'sources': [
        'platform/test_ax_node_wrapper.cc',
        'platform/test_ax_node_wrapper.h',
        'tree_generator.cc',
        'tree_generator.h',
      ]
    },
    {
      'target_name': 'accessibility_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'accessibility',
        'accessibility_test_support',
        'ax_gen',
      ],
      'sources': [
        'ax_generated_tree_unittest.cc',
        'ax_text_utils_unittest.cc',
        'ax_tree_combiner_unittest.cc',
        'ax_tree_serializer_unittest.cc',
        'ax_tree_unittest.cc',
        'platform/ax_platform_node_win_unittest.cc'
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../../third_party/iaccessible2/iaccessible2.gyp:iaccessible2'
          ],
        }],
      ],
    },
    {
      'target_name': 'ax_gen',
      'type': 'static_library',
      # This target exports a hard dependency because dependent targets may
      # include ax_enums.h, a generated header.
      'hard_dependency': 1,
      'dependencies': [
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations'
        ],
      'sources': [
        '<@(schema_files)',
      ],
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'schema_files': [
          'ax_enums.idl',
        ],
        'non_compiled_schema_files': [],
        'cc_dir': 'ui/accessibility',
        # TODO(dtseng): Change this once all files under ui/accessibility
        # namespaced under ui::ax.
        'root_namespace': 'ui',
      },
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'accessibility_unittests_run',
          'type': 'none',
          'dependencies': [
            'accessibility_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'accessibility_unittests.isolate',
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
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'ui_accessibility_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '<(DEPTH)/build/android/empty',
            'has_java_resources': 0,
            'never_lint': 1,
          },
          'dependencies': [
            'ax_enumerations_java',
          ],
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'ax_enumerations_java',
          'type': 'none',
          'variables': {
            'source_file': 'ax_enums.idl',
          },
          'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}
