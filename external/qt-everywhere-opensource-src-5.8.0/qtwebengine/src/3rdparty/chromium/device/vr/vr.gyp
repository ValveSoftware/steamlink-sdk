# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/vr:mojo_bindings
      'target_name': 'device_vr_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'vr_service.mojom',
      ],
    },
    {
      # GN version: //device/vr:mojo_bindings_blink
      'target_name': 'device_vr_mojo_bindings_for_blink',
      'type': 'static_library',
      'variables': {
        'for_blink': 'true',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'vr_service.mojom',
      ],
    },
  ],
}
