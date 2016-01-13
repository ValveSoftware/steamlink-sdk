# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../mojo/mojo.gyp:mojo_service_provider_bindings',
    '../skia/skia.gyp:skia',
    '../third_party/WebKit/public/blink.gyp:blink',
  ],
  'sources': [
    'worker/websharedworker_stub.cc',
    'worker/websharedworker_stub.h',
    'worker/websharedworkerclient_proxy.cc',
    'worker/websharedworkerclient_proxy.h',
    'worker/worker_main.cc',
    'worker/shared_worker_permission_client_proxy.cc',
    'worker/shared_worker_permission_client_proxy.h',
    'worker/worker_thread.cc',
    'worker/worker_thread.h',
    'worker/worker_webapplicationcachehost_impl.cc',
    'worker/worker_webapplicationcachehost_impl.h',
    'worker/worker_webkitplatformsupport_impl.cc',
    'worker/worker_webkitplatformsupport_impl.h',
  ],
  'include_dirs': [
    '..',
  ],
}
