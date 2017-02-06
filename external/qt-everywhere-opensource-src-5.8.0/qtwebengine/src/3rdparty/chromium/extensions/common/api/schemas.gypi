# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'chromium_code': 1,
    'schema_files': [
      'alarms.idl',
      'app_current_window_internal.idl',
      'app_runtime.idl',
      'app_view_guest_internal.json',
      'app_window.idl',
      'audio.idl',
      'bluetooth.idl',
      'bluetooth_private.idl',
      'bluetooth_socket.idl',
      'cast_channel.idl',
      'document_scan.idl',
      'display_source.idl',
      'dns.idl',
      'events.json',
      'extensions_manifest_types.json',
      'extension_options_internal.idl',
      'extension_view_internal.json',
      'extension_types.json',
      'guest_view_internal.json',
      'management.json',
      'hid.idl',
      'idle.json',
      'mime_handler_private.idl',
      'mime_handler_view_guest_internal.json',
      'mojo_private.idl',
      'networking_private.idl',
      'power.idl',
      'printer_provider.idl',
      'printer_provider_internal.idl',
      'runtime.json',
      'serial.idl',
      'socket.idl',
      'sockets_tcp.idl',
      'sockets_tcp_server.idl',
      'sockets_udp.idl',
      'storage.json',
      'system_cpu.idl',
      'system_display.idl',
      'system_memory.idl',
      'system_network.idl',
      'system_storage.idl',
      'test.json',
      'usb.idl',
      'virtual_keyboard_private.json',
      'web_request.json',
      'web_view_internal.json'
    ],
    # ChromeOS-specific schemas.
    'chromeos_schema_files': [
      'diagnostics.idl',
      'networking_config.idl',
      'vpn_provider.idl',
      'webcam_private.idl',
    ],
    'non_compiled_schema_files': [
      'web_request_internal.json',
    ],
    'conditions': [
      ['chromeos==1', {
        'schema_files': [
          '<@(chromeos_schema_files)',
        ],
      }]
    ],
    'cc_dir': 'extensions/common/api',
    'root_namespace': 'extensions::api::%(namespace)s',
    'bundle_name': '',
    'impl_dir_': 'extensions/browser/api',
  },
}
