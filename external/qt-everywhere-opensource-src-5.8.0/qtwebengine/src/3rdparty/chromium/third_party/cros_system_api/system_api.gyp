{
  'targets': [
    {
      'target_name': 'system_api-protos-gen',
      'type': 'none',
      'variables': {
        'proto_in_dir': 'dbus',
        'proto_out_dir': 'include/system_api/proto_bindings',
      },
      'sources': [
        '<(proto_in_dir)/mtp_storage_info.proto',
        '<(proto_in_dir)/mtp_file_entry.proto',
        '<(proto_in_dir)/field_trial_list.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-protos',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'system_api-protos-gen',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/include/system_api/proto_bindings/mtp_storage_info.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/system_api/proto_bindings/mtp_file_entry.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/system_api/proto_bindings/field_trial_list.pb.cc',
      ]
    },
    {
      'target_name': 'system_api-power_manager-protos-gen',
      'type': 'none',
      'variables': {
        'proto_in_dir': 'dbus/power_manager',
        'proto_out_dir': 'include/power_manager/proto_bindings',
      },
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
      'target_name': 'system_api-power_manager-protos',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'system_api-power_manager-protos-gen',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/include/power_manager/proto_bindings/suspend.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/power_manager/proto_bindings/input_event.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/power_manager/proto_bindings/peripheral_battery_status.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/power_manager/proto_bindings/policy.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/power_manager/proto_bindings/power_supply_properties.pb.cc',
      ]
    },
    {
      'target_name': 'system_api-cryptohome-protos-gen',
      'type': 'none',
      'variables': {
        'proto_in_dir': 'dbus/cryptohome',
        'proto_out_dir': 'include/cryptohome/proto_bindings',
      },
      'sources': [
        '<(proto_in_dir)/key.proto',
        '<(proto_in_dir)/rpc.proto',
        '<(proto_in_dir)/signed_secret.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-cryptohome-protos',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'system_api-cryptohome-protos-gen',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/include/cryptohome/proto_bindings/key.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/cryptohome/proto_bindings/rpc.pb.cc',
        '<(SHARED_INTERMEDIATE_DIR)/include/cryptohome/proto_bindings/signed_secret.pb.cc',
      ]
    },
  ]
}
