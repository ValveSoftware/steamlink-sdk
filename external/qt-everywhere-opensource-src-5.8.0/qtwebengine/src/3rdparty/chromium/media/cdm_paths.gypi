# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Common variables for media CDM.

# Naming and folder structure below are following the recommendation for Chrome
# components. Component-updated CDMs must follow the same recommendation.

# Note: This file must be in sync with cdm_paths.cc

# GN version: //media/cdm/ppapi/cdm_paths.gni
{
  'variables': {
    'variables': {
      'conditions': [
        # OS name for components is close to "<(OS)" but has some differences.
        # Explicitly define what we use to avoid confusion.
        ['OS == "linux" and chromeos == 1', {
          'component_os%': 'cros'
        }, 'OS == "linux"', {
          'component_os%': 'linux'
        }, 'OS == "win"', {
          'component_os%': 'win'
        }, 'OS == "mac"', {
          'component_os%': 'mac'
        }, {
          'component_os%': 'unsupported_platform'
        }],
        # Architecture name for components is close to "<(current_cpu)" but has
        # some differences. Explicitly define what we use to avoid confusion.
        ['target_arch == "ia32"', {
          'component_arch%': 'x86'
        }, 'target_arch == "x64"', {
          'component_arch%': 'x64'
        }, 'target_arch == "arm"', {
          'component_arch%': 'arm'
        }, {
          'component_arch%': 'unsupported_arch'
        }],
      ],
    },
    'conditions' : [
      # Only enable platform specific path for Win and Mac, where CDMs are
      # Chrome components.
      # TODO(xhwang): Improve how we enable platform specific path. See
      # http://crbug.com/468584
      ['( OS == "win" or OS == "mac") and (target_arch == "ia32" or target_arch == "x64")', {
        # Path of Clear Key and Widevine CDMs relative to the output dir.
        'widevine_cdm_path%': 'WidevineCdm/_platform_specific/<(component_os)_<(component_arch)',
        'clearkey_cdm_path%': 'ClearKeyCdm/_platform_specific/<(component_os)_<(component_arch)',
      }, {
        'widevine_cdm_path%': '.',
        'clearkey_cdm_path%': '.',
      }],
    ]
  },
}
