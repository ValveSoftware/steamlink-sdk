# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/certificate_transparency
      'target_name': 'certificate_transparency',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:safe_json',
        '../components/components.gyp:url_matcher',
        '../components/prefs/prefs.gyp:prefs',
        '../components/url_formatter/url_formatter.gyp:url_formatter',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'certificate_transparency/ct_policy_manager.cc',
        'certificate_transparency/ct_policy_manager.h',
        'certificate_transparency/log_proof_fetcher.h',
        'certificate_transparency/log_proof_fetcher.cc',
        'certificate_transparency/pref_names.cc',
        'certificate_transparency/pref_names.h',
        'certificate_transparency/single_tree_tracker.cc',
        'certificate_transparency/single_tree_tracker.h',
        'certificate_transparency/tree_state_tracker.h',
        'certificate_transparency/tree_state_tracker.cc',
      ],
    }
  ],
}
