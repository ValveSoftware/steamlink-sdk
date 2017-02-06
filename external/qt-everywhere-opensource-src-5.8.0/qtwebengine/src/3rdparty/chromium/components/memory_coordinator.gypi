# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/memory_coordinator/public/interfaces
      'target_name': 'memory_coordinator_mojo_bindings',
      'type': 'static_library',
      'sources': [
        'memory_coordinator/public/interfaces/child_memory_coordinator.mojom',
      ],
      'includes': [ '../mojo/mojom_bindings_generator.gypi' ],
    },
    {
      # GN version: //components/memory_coordinator/common
      'target_name': 'memory_coordinator_common',
      'type': 'none',
      'dependencies': [
        'memory_coordinator_mojo_bindings',
      ],
      'sources': [
        'memory_coordinator/common/memory_coordinator_client.h',
      ],
    },
    {
      # GN version: //components/memory_coordinator/child
      'target_name': 'memory_coordinator_child',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'memory_coordinator_common',
        'memory_coordinator_mojo_bindings',
      ],
      'sources': [
        'memory_coordinator/child/child_memory_coordinator_impl.cc',
        'memory_coordinator/child/child_memory_coordinator_impl.h',
      ],
    },
  ],
}
