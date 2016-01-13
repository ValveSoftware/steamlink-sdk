# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide an action to
# repackage Java libraries and embed them into your own distribution.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_package_java',
#   'type': 'none',
#   'variables': {
#     'input_jar': 'path_to_your.jar',
#     'pattern': 'com.example.library.**',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# The generated jar-file will be as defined in <(output_jar_file).
# Required variables:
#  input_jar - The source jar you want to jarjar.
#  pattern - a class name with optional wildcards.
#    ** will match against any valid class name substring.
#    To match a single package component (by excluding . from the match), a
#    single * may be used instead.
# Optional variables:
#  result_package - is a class name which can optionally reference the
#    substrings matched by the wildcards. A numbered reference is available for
#    every * or ** in the <pattern>, starting from left to right: @1, @2, etc.
#    A special @0 reference contains the entire matched class name.
#
# Note: This can not be used together with building a jar in your own target
# using src/build/java.gypi, since you will then have both jar files added to
# input_jars_paths through the use of the all_dependent_settings.
{
  'dependencies': [
    '<(DEPTH)/build/build_output_dirs_android.gyp:build_output_dirs'
  ],
  # This all_dependent_settings is used for java targets only. This will add the
  # <(output_jar_file) jar to the classpath of dependent java targets.
  'all_dependent_settings': {
    'variables': {
      'input_jars_paths': [ '<(output_jar_file)' ],
    },
  },
  'variables': {
    'output_jar_file': '<(PRODUCT_DIR)/lib.java/chromium_repackaged_<(_target_name).jar',
    'jarjar_build_file': '<(DEPTH)/third_party/jarjar/build.xml',
    'jarjar_javalib_file': '<(DEPTH)/third_party/jarjar/binary-distribution/jarjar-1.4.jar',
    'result_package%': 'org.chromium.repackaged.@0',
  },
  'actions': [
    {
      'action_name': 'ant_<(_target_name)',
      'message': 'Repackaging <(_target_name) from <(input_jar_file) to <(output_jar_file)',
      'inputs': [
        '<(jarjar_build_file)',
        '<(jarjar_javalib_file)',
        '<(input_jar_file)',
      ],
      'outputs': [
        '<(output_jar_file)',
      ],
      'action': [
        'ant',
        '-Dbasedir=.',
        '-DJARJAR_JAR=<(jarjar_javalib_file)',
        '-DINPUT_JAR=<(input_jar_file)',
        '-DOUTPUT_JAR=<(output_jar_file)',
        '-DPATTERN=<(pattern)',
        '-DRESULT_PACKAGE=<(result_package)',
        '-buildfile',
        '<(jarjar_build_file)'
      ]
    },
  ],
}
