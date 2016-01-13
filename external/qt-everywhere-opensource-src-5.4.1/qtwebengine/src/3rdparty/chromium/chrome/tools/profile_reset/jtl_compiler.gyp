{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'jtl_compiler',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',        
        '../../../crypto/crypto.gyp:crypto',
        'jtl_compiler_lib',
      ],
      'sources': [
        '../../browser/profile_resetter/jtl_foundation.cc',
        '../../browser/profile_resetter/jtl_foundation.h',
        'jtl_compiler_frontend.cc',
      ],
    },
    {
      'target_name': 'jtl_compiler_lib',
      'type': 'static_library',
      'product_name': 'jtl_compiler',
      'dependencies': [
        '../../../third_party/re2/re2.gyp:re2',
        '../../../base/base.gyp:base',
      ],
      'sources': [
        '../../browser/profile_resetter/jtl_foundation.h',
        '../../browser/profile_resetter/jtl_instructions.h',
        'jtl_compiler.h',
        'jtl_compiler.cc',
        'jtl_parser.h',
        'jtl_parser.cc',
      ],
    },
  ],
}
