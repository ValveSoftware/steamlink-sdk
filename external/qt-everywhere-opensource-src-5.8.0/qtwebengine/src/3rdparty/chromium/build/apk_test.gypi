# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build APK based test suites.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'test_suite_name_apk',
#   'type': 'none',
#   'variables': {
#     'test_suite_name': 'test_suite_name',  # string
#     'input_jars_paths': ['/path/to/test_suite.jar', ... ],  # list
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#

{
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base_java',
    '<(DEPTH)/build/android/pylib/device/commands/commands.gyp:chromium_commands',
    '<(DEPTH)/build/android/pylib/remote/device/dummy/dummy.gyp:require_remote_device_dummy_apk',
    '<(DEPTH)/testing/android/appurify_support.gyp:appurify_support_java',
    '<(DEPTH)/testing/android/on_device_instrumentation.gyp:reporter_java',
    '<(DEPTH)/testing/android/native_test.gyp:native_test_java',
    '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
  ],
  'conditions': [
     ['OS == "android"', {
       'variables': {
         # These are used to configure java_apk.gypi included below.
         'test_type': 'gtest',
         'apk_name': '<(test_suite_name)',
         'intermediate_dir': '<(PRODUCT_DIR)/<(test_suite_name)_apk',
         'generated_src_dirs': [ '<(SHARED_INTERMEDIATE_DIR)/<(test_suite_name)_jinja', ],
         'final_apk_path': '<(intermediate_dir)/<(test_suite_name)-debug.apk',
         'java_in_dir': '<(DEPTH)/build/android/empty',
         'native_lib_target': 'lib<(test_suite_name)',
         # TODO(yfriedman, cjhopman): Support managed installs for gtests.
         'gyp_managed_install': 0,
         'variables': {
           'use_native_activity%': "false",
           'android_manifest_path%': '',
         },
         'use_native_activity%': '<(use_native_activity)',
         'jinja_variables': [
           'native_library_name=<(test_suite_name)',
           'use_native_activity=<(use_native_activity)',
         ],
         'conditions': [
           ['component == "shared_library"', {
             'jinja_variables': [
               'is_component_build=true',
             ],
           }, {
             'jinja_variables': [
               'is_component_build=false',
             ],
           }],
           ['android_manifest_path == ""', {
             'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/<(test_suite_name)_jinja/AndroidManifest.xml',
             'manifest_template': '<(DEPTH)/testing/android/native_test/java',
           }, {
             'android_manifest_path%': '<(android_manifest_path)',
             'manifest_template': '',
           }],
         ],
       },
       'conditions': [
         ['manifest_template != ""', {
           'variables': {
             'jinja_inputs': '<(manifest_template)/AndroidManifest.xml.jinja2',
             'jinja_output': '<(android_manifest_path)',
           },
           'includes': ['android/jinja_template.gypi'],
         }],
       ],
       'includes': [ 'java_apk.gypi', 'android/test_runner.gypi' ],
     }],  # 'OS == "android"
  ],  # conditions
}
