# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/certificate_reporting
      'target_name': 'certificate_reporting',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'cert_logger_proto',
        'encrypted_cert_logger_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        "certificate_reporting/error_report.cc",
        "certificate_reporting/error_report.h",
        "certificate_reporting/error_reporter.cc",
        "certificate_reporting/error_reporter.h",
      ]
    },
    {
      # Protobuf compiler / generator for the certificate error reporting
      # protocol buffer.
      # GN version: //components/certificate_reporting:cert_logger_proto
      'target_name': 'cert_logger_proto',
      'type': 'static_library',
      'sources': [ 'certificate_reporting/cert_logger.proto', ],
      'variables': {
        'proto_in_dir': 'certificate_reporting/',
        'proto_out_dir': 'components/certificate_reporting/',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
    {
      # Protobuf compiler / generator for the encrypted certificate
      #  reports protocol buffer.
      # GN version: //components/certificate_reporting:encrypted_cert_logger_proto
      'target_name': 'encrypted_cert_logger_proto',
      'type': 'static_library',
      'sources': [ 'certificate_reporting/encrypted_cert_logger.proto', ],
      'variables': {
        'proto_in_dir': 'certificate_reporting/',
        'proto_out_dir': 'components/certificate_reporting/',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ]
}
