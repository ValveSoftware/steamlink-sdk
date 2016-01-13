# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['enable_plugins==1', {
      'dependencies': [
        '../base/base.gyp:base',
        '../mojo/mojo.gyp:mojo_service_provider_bindings',
        '../ppapi/ppapi_internal.gyp:ppapi_ipc',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'sources': [
        'ppapi_plugin/broker_process_dispatcher.cc',
        'ppapi_plugin/broker_process_dispatcher.h',
        'ppapi_plugin/plugin_process_dispatcher.cc',
        'ppapi_plugin/plugin_process_dispatcher.h',
        'ppapi_plugin/ppapi_broker_main.cc',
        'ppapi_plugin/ppapi_plugin_main.cc',
        'ppapi_plugin/ppapi_thread.cc',
        'ppapi_plugin/ppapi_thread.h',
        'ppapi_plugin/ppapi_webkitplatformsupport_impl.cc',
        'ppapi_plugin/ppapi_webkitplatformsupport_impl.h',
      ],
      'include_dirs': [
        '..',
      ],
    }],
  ],
}
