# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'url_matcher',
      'type': '<(component)',
      'dependencies': [
        '../third_party/re2/re2.gyp:re2',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'URL_MATCHER_IMPLEMENTATION',
      ],
      'sources': [
        'url_matcher/regex_set_matcher.cc',
        'url_matcher/regex_set_matcher.h',
        'url_matcher/string_pattern.cc',
        'url_matcher/string_pattern.h',
        'url_matcher/substring_set_matcher.cc',
        'url_matcher/substring_set_matcher.h',
        'url_matcher/url_matcher.cc',
        'url_matcher/url_matcher.h',
        'url_matcher/url_matcher_constants.cc',
        'url_matcher/url_matcher_constants.h',
        'url_matcher/url_matcher_export.h',
        'url_matcher/url_matcher_factory.cc',
        'url_matcher/url_matcher_factory.h',
        'url_matcher/url_matcher_helpers.cc',
        'url_matcher/url_matcher_helpers.h',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
