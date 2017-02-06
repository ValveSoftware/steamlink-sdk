# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    # Set this to run the jscompile checks after building the webapp.
    'run_jscompile%': 0,

    # Set this to use GCD instead of the remoting directory service.
    'remoting_use_gcd%': 0,

    # Set this to enable Android Chromoting Cardboard Activity.
    'enable_cardboard%': 0,

    'variables': {
      'conditions': [
        # Enable the multi-process host on Windows by default.
        ['OS=="win"', {
          'remoting_multi_process%': 1,
        }, {
          'remoting_multi_process%': 0,
        }],
      ],
    },
    'remoting_multi_process%': '<(remoting_multi_process)',

    'remoting_rdp_session%': 1,

    'branding_path': '../remoting/branding_<(branding)',

    # The ar_service_environment variable is used to define the target
    # environment for the app being built.
    # The allowed values are dev and prod.
    'conditions': [
      ['buildtype == "Dev"', {
        'ar_service_environment%': 'dev',
      }, {  # buildtype != 'Dev'
        # Non-dev builds should default to 'prod'.
        'ar_service_environment%': 'prod',
      }],
    ],  # conditions

  },
}
