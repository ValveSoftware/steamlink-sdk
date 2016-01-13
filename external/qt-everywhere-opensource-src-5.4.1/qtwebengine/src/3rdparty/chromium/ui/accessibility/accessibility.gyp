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
        'ax_tree_serializer.cc',
        'ax_tree_serializer.h',
        'ax_tree_source.h',
        'ax_tree_update.cc',
        'ax_tree_update.h',
        'ax_view_state.cc',
        'ax_view_state.h',
      ]
    },
    {
      'target_name': 'accessibility_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'accessibility',
        'ax_gen',
      ],
      'sources': [
        'ax_generated_tree_unittest.cc',
        'ax_tree_serializer_unittest.cc',
        'ax_tree_unittest.cc',
      ]
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
        '../../build/json_schema_bundle_compile.gypi',
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
        'root_namespace': '',
      },
    },
  ],
}
