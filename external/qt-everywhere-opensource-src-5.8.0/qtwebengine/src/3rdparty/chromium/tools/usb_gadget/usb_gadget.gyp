# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'usb_gadget',
      'type': 'none',
      'variables': {
        'usb_gadget_files': [
          '__init__.py',
          '__main__.py',
          'composite_echo_gadget.py',
          'composite_gadget.py',
          'default_gadget.py',
          'echo_gadget.py',
          'gadget.py',
          'hid_constants.py',
          'hid_descriptors.py',
          'hid_echo_gadget.py',
          'hid_gadget.py',
          'keyboard_gadget.py',
          'linux_gadgetfs.py',
          'mouse_gadget.py',
          'msos20_descriptors.py',
          'server.py',
          'usb_constants.py',
          'usb_descriptors.py',
        ],
        'usb_gadget_package': '<(PRODUCT_DIR)/usb_gadget.zip',
        'usb_gadget_package_hash': '<(PRODUCT_DIR)/usb_gadget.zip.md5',
      },
      'actions': [
        {
          'action_name': 'Building USB Gadget ZIP bundle',
          'inputs': [
            'package.py',
            '<@(usb_gadget_files)',
          ],
          'outputs': [
            '<(usb_gadget_package)',
            '<(usb_gadget_package_hash)',
          ],
          'action': [
            'python', 'package.py',
            '--zip-file', '<(usb_gadget_package)',
            '--hash-file', '<(usb_gadget_package_hash)',
            '<@(usb_gadget_files)',
          ]
        }
      ]
    }
  ]
}
