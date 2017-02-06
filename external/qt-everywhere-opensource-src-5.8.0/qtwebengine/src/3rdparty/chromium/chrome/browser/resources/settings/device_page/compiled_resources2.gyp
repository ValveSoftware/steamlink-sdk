# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'device_page',
      'dependencies': [
        '../settings_page/compiled_resources2.gyp:settings_animated_pages',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'device_page_browser_proxy',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'touchpad',
      'dependencies': [
        'device_page_browser_proxy'
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'keyboard',
      'dependencies': [
        '../prefs/compiled_resources2.gyp:prefs_types',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(EXTERNS_GYP):settings_private',
        'device_page_browser_proxy'
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'display',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-button/compiled_resources2.gyp:paper-button-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-slider/compiled_resources2.gyp:paper-slider-extracted',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:i18n_behavior',
        '<(EXTERNS_GYP):system_display',
        '<(INTERFACES_GYP):system_display_interface',
        'display_layout'
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'display_layout',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-resizable-behavior/compiled_resources2.gyp:iron-resizable-behavior-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-button/compiled_resources2.gyp:paper-button-extracted',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(EXTERNS_GYP):system_display',
        '<(INTERFACES_GYP):system_display_interface',
        'drag_behavior'
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'display_overscan_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(EXTERNS_GYP):system_display',
        '<(INTERFACES_GYP):system_display_interface',
        'display'
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    }, 
    {
      'target_name': 'drag_behavior',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
 ],
}
