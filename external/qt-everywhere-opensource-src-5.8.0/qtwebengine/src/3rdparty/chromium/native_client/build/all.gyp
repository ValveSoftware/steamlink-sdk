{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'pull_in_all',
      'type': 'none',
      'dependencies': [
        '../src/nonsfi/loader/loader.gyp:*',
        '../src/shared/gio/gio.gyp:*',
        '../src/shared/imc/imc.gyp:*',
        '../src/shared/platform/platform.gyp:*',
        '../src/trusted/cpu_features/cpu_features.gyp:*',
        '../src/trusted/debug_stub/debug_stub.gyp:*',
        '../src/trusted/desc/desc.gyp:*',
        '../src/trusted/nacl_base/nacl_base.gyp:*',
        '../src/trusted/perf_counter/perf_counter.gyp:*',
        '../src/trusted/platform_qualify/platform_qualify.gyp:*',
        '../src/trusted/service_runtime/service_runtime.gyp:*',
        '../src/untrusted/elf_loader/elf_loader.gyp:*',
        '../src/untrusted/irt/irt.gyp:irt_core_nexe',
        '../src/untrusted/minidump_generator/minidump_generator.gyp:*',
        '../src/untrusted/nacl/nacl.gyp:*',
        '../src/untrusted/nosys/nosys.gyp:*',
        '../src/untrusted/pthread/pthread.gyp:*',
        '../tests.gyp:*',
        '../tests/sel_main_chrome/sel_main_chrome.gyp:*',
        'nacl_core_sdk.gyp:*',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'dependencies': [
            '../src/trusted/validator_arm/validator_arm.gyp:*',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            '../src/trusted/validator/driver/ncval.gyp:*',
            '../src/trusted/validator_arm/ncval.gyp:*',
          ],
        }],
        ['target_arch=="mipsel"', {
          'dependencies': [
            '../src/trusted/validator_mips/validator_mips.gyp:*',
          ],
        }],
      ],
    },
  ],
}
