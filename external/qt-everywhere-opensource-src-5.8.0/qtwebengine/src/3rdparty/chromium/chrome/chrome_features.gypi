# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is the GYP equivalent of //chrome/common/features.gni.
# Please keep in sync!

{
  'variables': {
    'variables': {
      # Conditional variables.
      'conditions': [
        ['OS=="android"', {
          'android_java_ui%': 1,
        }, {
          'android_java_ui': 0,
        }],
        ['OS=="android" or OS=="ios"', {
          'enable_background%': 0,
          'enable_google_now%': 0,
        }, {
          'enable_background%': 1,
          'enable_google_now%': 1,
        }]
      ],

      # Use vulcanized HTML/CSS/JS resources to speed up WebUI (chrome://)
      # pages. https://github.com/polymer/vulcanize
      'use_vulcanize%': 1,
    },

    # Anything in the conditions needs to be copied to the outer scope to be
    # accessible.
    'enable_background%': '<(enable_background)',
    'enable_google_now%': '<(enable_google_now)',
    'android_java_ui%': '<(android_java_ui)',
    'use_vulcanize%': '<(use_vulcanize)',

    # GN only, but defined here so BUILDFLAG works without ifdef.
    'enable_package_mash_services%': 0,

    # Grit defines based on the feature flags. These must be manually added to
    # grit targets.
    'chrome_grit_defines': [
      '-D', 'enable_background=<(enable_background)',
      '-D', 'enable_google_now=<(enable_google_now)',
      '-D', 'use_vulcanize=<(use_vulcanize)',
    ],
  },
}
