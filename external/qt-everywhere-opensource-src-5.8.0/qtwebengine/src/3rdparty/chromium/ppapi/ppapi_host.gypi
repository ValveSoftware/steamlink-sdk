# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
   'targets': [
    {
      # GN version: //ppapi/host
      'target_name': 'ppapi_host',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:shared_memory_support',
        '../ui/surface/surface.gyp:surface',
        '../url/url.gyp:url_lib',
        'ppapi.gyp:ppapi_c',
        'ppapi_internal.gyp:ppapi_ipc',
        'ppapi_internal.gyp:ppapi_proxy',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'defines': [
        'PPAPI_HOST_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'host/dispatch_host_message.h',
        'host/error_conversion.cc',
        'host/error_conversion.h',
        'host/host_factory.h',
        'host/host_message_context.cc',
        'host/host_message_context.h',
        'host/instance_message_filter.cc',
        'host/instance_message_filter.h',
        'host/message_filter_host.cc',
        'host/message_filter_host.h',
        'host/ppapi_host.cc',
        'host/ppapi_host.h',
        'host/ppapi_host_export.h',
        'host/resource_host.cc',
        'host/resource_host.h',
        'host/resource_message_filter.cc',
        'host/resource_message_filter.h',
        'host/resource_message_handler.cc',
        'host/resource_message_handler.h',
      ],
    },
  ],
}
