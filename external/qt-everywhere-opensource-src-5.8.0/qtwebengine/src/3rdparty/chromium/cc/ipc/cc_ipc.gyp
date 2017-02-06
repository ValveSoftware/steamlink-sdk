# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
     {
      # GN version: //cc/ipc
      'target_name': 'cc_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../gpu/gpu.gyp:gpu_ipc_common',
        '../../ipc/ipc.gyp:ipc',
        '../../skia/skia.gyp:skia',
        '../../ui/events/events.gyp:events_base',
        '../../ui/events/events.gyp:events_ipc',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '../../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '../../ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
      ],
      'defines': [
        'CC_IPC_IMPLEMENTATION',
      ],
      'sources': [
        'cc_param_traits.cc',
        'cc_param_traits.h',
        'cc_param_traits_macros.h',
      ],
    },
    {
      # GN version: //cc/ipc:interfaces
      # TODO: Add begin_frame_args, render_pass_id, returned_resources and
      # transferable_resource .mojom and .typemap files here so that this target
      # is consistent with that in GN build.
      'target_name': 'interfaces',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'surface_id.mojom',
          'surface_sequence.mojom',
        ],
        'mojom_typemaps': [
          'surface_id.typemap',
          'surface_sequence.typemap',
        ],
      },
      'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //cc/ipc:interfaces
      # Same target as above except that this is used in Blink
      'target_name': 'interfaces_blink',
      'type': 'none',
      'variables': {
        'for_blink': 'true',
        'mojom_files': [
          'surface_id.mojom',
          'surface_sequence.mojom',
        ],
        'mojom_typemaps': [
          'surface_id.typemap',
          'surface_sequence.typemap',
        ],
      },
      'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
    },

  ],
}
