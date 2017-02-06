# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# We have 2 separate browser targets because //components/html_viewer requires
# startup_metric_utils_browser, but has symbols that conflict with mojo symbols
# that startup_metric_utils_browser_host indirectly depends on.

{
  'targets': [
    {
      # GN version: //components/startup_metric_utils/browser:lib
      'target_name': 'startup_metric_utils_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'components.gyp:version_info',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'startup_metric_utils/browser/pref_names.cc',
        'startup_metric_utils/browser/pref_names.h',
        'startup_metric_utils/browser/startup_metric_utils.cc',
        'startup_metric_utils/browser/startup_metric_utils.h',
      ],
    },
    {
      # GN version: //components/startup_metric_utils/browser:host
      'target_name': 'startup_metric_utils_browser_host',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        'startup_metric_utils_browser',
        'startup_metric_utils_interfaces'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'startup_metric_utils/browser/startup_metric_host_impl.cc',
        'startup_metric_utils/browser/startup_metric_host_impl.h',
      ],
    },
    {
      # GN version: //components/startup_metric_utils/common:interfaces
      'target_name': 'startup_metric_utils_interfaces',
      'type': 'static_library',
      'sources': [
        'startup_metric_utils/common/startup_metric.mojom',
      ],
      'dependencies': [
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_custom_types_mojom',
      ],
      'variables': {
        'mojom_typemaps': [
          '<(DEPTH)/mojo/common/common_custom_types.typemap',
        ],
      },
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
    },
  ],
  'conditions': [
    ['OS == "win"', {
      'targets': [
        {
          # GN version: //components/startup_metric_utils/common
          'target_name': 'startup_metric_utils_win',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            'components.gyp:variations',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'startup_metric_utils/common/pre_read_field_trial_utils_win.cc',
            'startup_metric_utils/common/pre_read_field_trial_utils_win.h',
          ],
        },
      ],
    }],
  ],
}
