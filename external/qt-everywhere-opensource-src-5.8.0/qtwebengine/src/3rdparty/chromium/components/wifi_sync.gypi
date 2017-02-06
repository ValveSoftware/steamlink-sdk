# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/wifi_sync
      'target_name': 'wifi_sync',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:onc_component',
        '../sync/sync.gyp:sync',
      ],
      'sources': [
        'wifi_sync/network_state_helper_chromeos.cc',
        'wifi_sync/network_state_helper_chromeos.h',
        'wifi_sync/wifi_config_delegate.h',
        'wifi_sync/wifi_config_delegate_chromeos.cc',
        'wifi_sync/wifi_config_delegate_chromeos.h',
        'wifi_sync/wifi_credential.cc',
        'wifi_sync/wifi_credential.h',
        'wifi_sync/wifi_credential_syncable_service.cc',
        'wifi_sync/wifi_credential_syncable_service.h',
        'wifi_sync/wifi_credential_syncable_service_factory.cc',
        'wifi_sync/wifi_credential_syncable_service_factory.h',
        'wifi_sync/wifi_security_class.cc',
        'wifi_sync/wifi_security_class.h',
        'wifi_sync/wifi_security_class_chromeos.cc',
      ],
    },
  ],
}
