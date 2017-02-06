# Copyright 2014  The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
    'targets': [
        {
            # GN version: //third_party/dom_distiller_js:proto
            'target_name': 'dom_distiller_js_proto',
            'type': 'static_library',
            'sources': [ 'dist/proto/dom_distiller.proto', ],
            'variables': {
              'proto_in_dir': 'dist/proto',
              'proto_out_dir': 'third_party/dom_distiller_js',
            },
            'direct_dependent_settings': {
              'include_dirs': ['dist/proto_gen'],
            },
            'includes': [ '../../build/protoc.gypi', ],
        }
    ]
}

