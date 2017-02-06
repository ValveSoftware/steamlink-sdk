# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copresence',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/webrtc/common_audio/common_audio.gyp:common_audio',
        'audio_modem',
        'copresence_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'copresence/copresence_manager_impl.cc',
        'copresence/copresence_state_impl.cc',
        'copresence/copresence_state_impl.h',
        'copresence/copresence_switches.cc',
        'copresence/copresence_switches.h',
        'copresence/handlers/audio/audio_directive_handler.h',
        'copresence/handlers/audio/audio_directive_handler_impl.cc',
        'copresence/handlers/audio/audio_directive_handler_impl.h',
        'copresence/handlers/audio/audio_directive_list.cc',
        'copresence/handlers/audio/audio_directive_list.h',
        'copresence/handlers/audio/tick_clock_ref_counted.cc',
        'copresence/handlers/audio/tick_clock_ref_counted.h',
        'copresence/handlers/directive_handler.h',
        'copresence/handlers/directive_handler_impl.cc',
        'copresence/handlers/directive_handler_impl.h',
        'copresence/handlers/gcm_handler.h',
        'copresence/handlers/gcm_handler_impl.cc',
        'copresence/handlers/gcm_handler_impl.h',
        'copresence/public/copresence_constants.h',
        'copresence/public/copresence_delegate.h',
        'copresence/public/copresence_manager.h',
        'copresence/public/copresence_observer.h',
        'copresence/public/copresence_state.h',
        'copresence/rpc/http_post.cc',
        'copresence/rpc/http_post.h',
        'copresence/rpc/rpc_handler.cc',
        'copresence/rpc/rpc_handler.h',
        'copresence/timed_map.h',
        'copresence/tokens.cc',
        'copresence/tokens.h',
      ],
      'export_dependent_settings': [
        'copresence_proto',
      ],
    },
    {
      'target_name': 'copresence_test_support',
      'type': 'static_library',
      'dependencies': [
        'copresence_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'copresence/test/fake_directive_handler.cc',
        'copresence/test/fake_directive_handler.h',
      ],
      'export_dependent_settings': [
        'copresence_proto',
      ],
    },
    {
      # Protobuf compiler / generate rule for copresence.
      # GN version: //components/copresence/proto
      # Note: These protos are auto-generated from the protos of the
      # Copresence server. Currently this strips all formatting and comments
      # but makes sure that we are always using up to date protos.
      'target_name': 'copresence_proto',
      'type': 'static_library',
      'sources': [
        'copresence/proto/codes.proto',
        'copresence/proto/config_data.proto',
        'copresence/proto/data.proto',
        'copresence/proto/enums.proto',
        'copresence/proto/identity.proto',
        'copresence/proto/push_message.proto',
        'copresence/proto/rpcs.proto',
      ],
      'variables': {
        'proto_in_dir': 'copresence/proto',
        'proto_out_dir': 'components/copresence/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
