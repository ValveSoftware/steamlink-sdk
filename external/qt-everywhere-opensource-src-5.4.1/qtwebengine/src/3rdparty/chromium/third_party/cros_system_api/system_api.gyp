{
  'targets': [
    {
      'target_name': 'system_api-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'dbus',
        'proto_out_dir': 'include/system_api/proto_bindings',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/mtp_storage_info.proto',
        '<(proto_in_dir)/mtp_file_entry.proto',
        '<(proto_in_dir)/field_trial_list.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-power_manager-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'dbus/power_manager',
        'proto_out_dir': 'include/power_manager/proto_bindings',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/suspend.proto',
        '<(proto_in_dir)/input_event.proto',
        '<(proto_in_dir)/peripheral_battery_status.proto',
        '<(proto_in_dir)/policy.proto',
        '<(proto_in_dir)/power_supply_properties.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-cryptohome-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'dbus/cryptohome',
        'proto_out_dir': 'include/cryptohome/proto_bindings',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/key.proto',
        '<(proto_in_dir)/rpc.proto',
        '<(proto_in_dir)/signed_secret.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-headers',
      'type': 'none',
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/include/chromeos/dbus',
          'files': [
            'dbus/service_constants.h'
          ]
        }
      ]
    }
  ]
}
