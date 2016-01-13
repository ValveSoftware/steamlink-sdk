# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_protocol_constants',
      'type': 'none',
      'actions': [
        {
          'action_name': 'devtools_protocol_constants',
          'variables': {
            'blink_protocol': '../../../third_party/WebKit/Source/devtools/protocol.json',
            'generator': '../../../content/public/browser/devtools_protocol_constants_generator.py',
            'package': 'chrome'
          },
          'inputs': [
            '<(blink_protocol)',
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
          ],
          'message': 'Generating DevTools protocol constants from <(blink_protocol)'
        }
      ],
    },
  ],
}
