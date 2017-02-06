# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'internet_page',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(INTERFACES_GYP):networking_private_interface',
        '../settings_page/compiled_resources2.gyp:settings_animated_pages',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'internet_detail_page',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/cr_elements/policy/compiled_resources2.gyp:cr_policy_network_behavior',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):networking_private',
        '<(INTERFACES_GYP):networking_private_interface',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'internet_known_networks_page',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):networking_private',
        '<(INTERFACES_GYP):networking_private_interface',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_apnlist',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_ip_config',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_nameservers',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_property_list',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/cr_elements/policy/compiled_resources2.gyp:cr_policy_network_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_proxy',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/cr_elements/policy/compiled_resources2.gyp:cr_policy_network_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_proxy_input',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_proxy_exclusions',
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_siminfo',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(INTERFACES_GYP):networking_private_interface',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_summary',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(INTERFACES_GYP):networking_private_interface',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'network_summary_item',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/network/compiled_resources2.gyp:cr_onc_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
