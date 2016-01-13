# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    #
    # ppp_entrypoints_sources is a set of source files that provide
    # a default implementation of plugin entry-points as defined in
    # ppp.h. Note that these source files assume that plugin is dependent
    # on ppapi_cpp_objects target defined in ppapi.gyp.
    #
    # Note that we cannot compile these source files into a static library
    # because the entry points need to be exported from the plugin.
    #
    'ppp_entrypoints_sources': [
      '<(DEPTH)/ppapi/cpp/module_embedder.h',
      '<(DEPTH)/ppapi/cpp/ppp_entrypoints.cc',
    ],
  },
}
