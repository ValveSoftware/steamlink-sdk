# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this .gypi in your target to dynamically set the
# use_official_google_api_keys variable (unless it is already
# explicitly set) and the associated preprocessor define.

{
  'variables': {
    'variables': {
      # See documentation of this variable in //build/common.gypi.
      'use_official_google_api_keys%': 2,
    },

    # Copy conditionally-set variables out one scope.
    'use_official_google_api_keys%': '<(use_official_google_api_keys)',

    'conditions': [
      # If use_official_google_api_keys is already set (to 0 or 1), we
      # do none of the implicit checking.  If it is set to 1 and the
      # internal keys file is missing, the build will fail at compile
      # time.  If it is set to 0 and keys are not provided by other
      # means, a warning will be printed at compile time.
      ['use_official_google_api_keys==2', {
          'use_official_google_api_keys%':
            '<!(python <(DEPTH)/google_apis/build/check_internal.py <(DEPTH)/google_apis/internal/google_chrome_api_keys.h)',
        }],
    ]
  },

  'conditions': [
    ['use_official_google_api_keys==1', {
        'defines': ['USE_OFFICIAL_GOOGLE_API_KEYS=1'],
      }],
  ],
}
