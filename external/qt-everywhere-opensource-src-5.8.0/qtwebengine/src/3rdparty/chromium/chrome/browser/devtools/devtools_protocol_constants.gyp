# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_protocol_constants',
      'type': 'none',
      'dependencies': [
        '../../../third_party/WebKit/Source/core/inspector/inspector.gyp:protocol_version'
      ],
      'actions': [
        {
          'action_name': 'devtools_protocol_constants',
          'variables': {
            'blink_protocol': '<(SHARED_INTERMEDIATE_DIR)/blink/core/inspector/protocol.json',
            'browser_protocol': '../../../content/browser/devtools/browser_protocol.json',
            'generator': 'devtools_protocol_constants_generator.py',
            'package': 'chrome'
          },
          'inputs': [
            '<(blink_protocol)',
            '<(browser_protocol)',
            '<(generator)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.h'
          ],
          'action':[
            'python',
            '<(generator)',
            '<(package)',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.h',
            '<(blink_protocol)',
            '<(browser_protocol)',
          ],
          'message': 'Generating DevTools protocol constants from <(blink_protocol) and <(browser_protocol)'
        }
      ],
    },
  ],
}
