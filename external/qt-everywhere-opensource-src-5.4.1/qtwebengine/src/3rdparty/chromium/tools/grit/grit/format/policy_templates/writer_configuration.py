#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetConfigurationForBuild(defines):
  '''Returns a configuration dictionary for the given build that contains
  build-specific settings and information.

  Args:
    defines: Definitions coming from the build system.

  Raises:
    Exception: If 'defines' contains an unknown build-type.
  '''
  # The prefix of key names in config determines which writer will use their
  # corresponding values:
  #   win: Both ADM and ADMX.
  #   mac: Only plist.
  #   admx: Only ADMX.
  #   none/other: Used by all the writers.
  if '_chromium' in defines:
    config = {
      'build': 'chromium',
      'app_name': 'Chromium',
      'frame_name': 'Chromium Frame',
      'os_name': 'Chromium OS',
      'win_reg_mandatory_key_name': 'Software\\Policies\\Chromium',
      'win_reg_recommended_key_name':
          'Software\\Policies\\Chromium\\Recommended',
      'win_mandatory_category_path': ['chromium'],
      'win_recommended_category_path': ['chromium_recommended'],
      'admx_namespace': 'Chromium.Policies.Chromium',
      'admx_prefix': 'chromium',
    }
  elif '_google_chrome' in defines:
    config = {
      'build': 'chrome',
      'app_name': 'Google Chrome',
      'frame_name': 'Google Chrome Frame',
      'os_name': 'Google Chrome OS',
      'win_reg_mandatory_key_name': 'Software\\Policies\\Google\\Chrome',
      'win_reg_recommended_key_name':
          'Software\\Policies\\Google\\Chrome\\Recommended',
      'win_mandatory_category_path': ['google', 'googlechrome'],
      'win_recommended_category_path': ['google', 'googlechrome_recommended'],
      'admx_namespace': 'Google.Policies.Chrome',
      'admx_prefix': 'chrome',
    }
  else:
    raise Exception('Unknown build')
  config['win_group_policy_class'] = 'Both'
  config['win_supported_os'] = 'SUPPORTED_WINXPSP2'
  if 'mac_bundle_id' in defines:
    config['mac_bundle_id'] = defines['mac_bundle_id']
  return config
