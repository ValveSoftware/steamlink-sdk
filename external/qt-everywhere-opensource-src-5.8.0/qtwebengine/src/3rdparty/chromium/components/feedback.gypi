# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'feedback_component',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../net/net.gyp:net',
        '../third_party/re2/re2.gyp:re2',
        '../third_party/zlib/google/zip.gyp:zip',
        'keyed_service_core',
        'feedback_proto',
        'components.gyp:variations_net',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
      ],
      'sources': [
        'feedback/anonymizer_tool.cc',
        'feedback/anonymizer_tool.h',
        'feedback/feedback_common.cc',
        'feedback/feedback_common.h',
        'feedback/feedback_data.cc',
        'feedback/feedback_data.h',
        'feedback/feedback_report.cc',
        'feedback/feedback_report.h',
        'feedback/feedback_switches.cc',
        'feedback/feedback_switches.h',
        'feedback/feedback_uploader.cc',
        'feedback/feedback_uploader.h',
        'feedback/feedback_uploader_chrome.cc',
        'feedback/feedback_uploader_chrome.h',
        'feedback/feedback_uploader_delegate.cc',
        'feedback/feedback_uploader_delegate.h',
        'feedback/feedback_uploader_factory.cc',
        'feedback/feedback_uploader_factory.h',
        'feedback/feedback_util.cc',
        'feedback/feedback_util.h',
        'feedback/tracing_manager.cc',
        'feedback/tracing_manager.h',
      ],
    },
    {
      # Protobuf compiler / generate rule for feedback
      # GN version: //components/feedback/proto
      'target_name': 'feedback_proto',
      'type': 'static_library',
      'sources': [
        'feedback/proto/annotations.proto',
        'feedback/proto/chrome.proto',
        'feedback/proto/common.proto',
        'feedback/proto/dom.proto',
        'feedback/proto/extension.proto',
        'feedback/proto/math.proto',
        'feedback/proto/web.proto',
      ],
      'variables': {
        'proto_in_dir': 'feedback/proto',
        'proto_out_dir': 'components/feedback/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
