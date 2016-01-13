# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['disable_nacl==1', {
        'nacl_defines': [
        ],
      }, {
        'conditions': [
          ['OS=="win"', {
            'nacl_defines': [
              'NACL_WINDOWS=1',
              'NACL_LINUX=0',
              'NACL_OSX=0',
              'NACL_ANDROID=0',
            ],
          }],
          ['OS=="linux"', {
            'nacl_defines': [
              'NACL_WINDOWS=0',
              'NACL_LINUX=1',
              'NACL_OSX=0',
              'NACL_ANDROID=0',
            ],
          }],
          ['OS=="mac"', {
            'nacl_defines': [
              'NACL_WINDOWS=0',
              'NACL_LINUX=0',
              'NACL_OSX=1',
              'NACL_ANDROID=0',
            ],
          }],
          ['OS=="android"', {
            'nacl_defines': [
              'NACL_WINDOWS=0',
              'NACL_LINUX=1',
              'NACL_OSX=0',
              'NACL_ANDROID=1',
            ],
          }],
        ],
      }],
      # TODO(mcgrathr): This duplicates native_client/build/common.gypi;
      # we should figure out a way to unify the settings.
      ['target_arch=="ia32"', {
        'nacl_defines': [
          'NACL_TARGET_SUBARCH=32',
          'NACL_TARGET_ARCH=x86',
          'NACL_BUILD_SUBARCH=32',
          'NACL_BUILD_ARCH=x86',
        ],
      }],
      ['target_arch=="x64"', {
        'nacl_defines': [
          'NACL_TARGET_SUBARCH=64',
          'NACL_TARGET_ARCH=x86',
          'NACL_BUILD_SUBARCH=64',
          'NACL_BUILD_ARCH=x86',
        ],
      }],
      ['target_arch=="arm"', {
        'nacl_defines': [
          'NACL_BUILD_ARCH=arm',
          'NACL_BUILD_SUBARCH=32',
          'NACL_TARGET_ARCH=arm',
          'NACL_TARGET_SUBARCH=32',
        ],
      }],
      ['target_arch=="mipsel"', {
        'nacl_defines': [
          'NACL_BUILD_ARCH=mips',
          'NACL_BUILD_SUBARCH=32',
          'NACL_TARGET_ARCH=mips',
          'NACL_TARGET_SUBARCH=32',
        ],
      }],
    ],
  }
}
