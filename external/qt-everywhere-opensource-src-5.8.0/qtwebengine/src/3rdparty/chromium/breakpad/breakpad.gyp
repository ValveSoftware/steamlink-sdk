# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'breakpad_sender.gypi',
    'breakpad_handler.gypi',
  ],
  'conditions': [
    ['OS!="win"', {
      'targets': [
        {
          # code shared by both {micro,mini}dump_stackwalk
          # GN version: //breakpad:stackwalk_common
          'target_name': 'stackwalk_common',
          'type': 'static_library',
          'includes': ['breakpad_tools.gypi'],
          'defines': ['BPLOG_MINIMUM_SEVERITY=SEVERITY_ERROR'],
          'sources': [
            'src/processor/basic_code_module.h',
            'src/processor/basic_code_modules.cc',
            'src/processor/basic_code_modules.h',
            'src/processor/basic_source_line_resolver.cc',
            'src/processor/call_stack.cc',
            'src/processor/cfi_frame_info.cc',
            'src/processor/cfi_frame_info.h',
            'src/processor/disassembler_x86.cc',
            'src/processor/disassembler_x86.h',
            'src/processor/dump_context.cc',
            'src/processor/dump_object.cc',
            'src/processor/logging.cc',
            'src/processor/logging.h',
            'src/processor/pathname_stripper.cc',
            'src/processor/pathname_stripper.h',
            'src/processor/process_state.cc',
            'src/processor/proc_maps_linux.cc',
            'src/processor/simple_symbol_supplier.cc',
            'src/processor/simple_symbol_supplier.h',
            'src/processor/source_line_resolver_base.cc',
            'src/processor/stack_frame_cpu.cc',
            'src/processor/stack_frame_symbolizer.cc',
            'src/processor/stackwalk_common.cc',
            'src/processor/stackwalker.cc',
            'src/processor/stackwalker_amd64.cc',
            'src/processor/stackwalker_amd64.h',
            'src/processor/stackwalker_arm.cc',
            'src/processor/stackwalker_arm.h',
            'src/processor/stackwalker_arm64.cc',
            'src/processor/stackwalker_arm64.h',
            'src/processor/stackwalker_mips.cc',
            'src/processor/stackwalker_mips.h',
            'src/processor/stackwalker_ppc.cc',
            'src/processor/stackwalker_ppc.h',
            'src/processor/stackwalker_ppc64.cc',
            'src/processor/stackwalker_ppc64.h',
            'src/processor/stackwalker_sparc.cc',
            'src/processor/stackwalker_sparc.h',
            'src/processor/stackwalker_x86.cc',
            'src/processor/stackwalker_x86.h',
            'src/processor/tokenize.cc',
            'src/processor/tokenize.h',
            # libdisasm
            'src/third_party/libdisasm/ia32_implicit.c',
            'src/third_party/libdisasm/ia32_implicit.h',
            'src/third_party/libdisasm/ia32_insn.c',
            'src/third_party/libdisasm/ia32_insn.h',
            'src/third_party/libdisasm/ia32_invariant.c',
            'src/third_party/libdisasm/ia32_invariant.h',
            'src/third_party/libdisasm/ia32_modrm.c',
            'src/third_party/libdisasm/ia32_modrm.h',
            'src/third_party/libdisasm/ia32_opcode_tables.c',
            'src/third_party/libdisasm/ia32_opcode_tables.h',
            'src/third_party/libdisasm/ia32_operand.c',
            'src/third_party/libdisasm/ia32_operand.h',
            'src/third_party/libdisasm/ia32_reg.c',
            'src/third_party/libdisasm/ia32_reg.h',
            'src/third_party/libdisasm/ia32_settings.c',
            'src/third_party/libdisasm/ia32_settings.h',
            'src/third_party/libdisasm/libdis.h',
            'src/third_party/libdisasm/qword.h',
            'src/third_party/libdisasm/x86_disasm.c',
            'src/third_party/libdisasm/x86_format.c',
            'src/third_party/libdisasm/x86_imm.c',
            'src/third_party/libdisasm/x86_imm.h',
            'src/third_party/libdisasm/x86_insn.c',
            'src/third_party/libdisasm/x86_misc.c',
            'src/third_party/libdisasm/x86_operand_list.c',
            'src/third_party/libdisasm/x86_operand_list.h',
          ],
          'conditions': [
            ['OS=="ios"', {
              'toolsets': ['host'],
            }],
          ],
        },
        {
          # GN version: //breakpad:microdump_stackwalk
          'target_name': 'microdump_stackwalk',
          'type': 'executable',
          'dependencies': ['stackwalk_common'],
          'includes': ['breakpad_tools.gypi'],
          'defines': ['BPLOG_MINIMUM_SEVERITY=SEVERITY_ERROR'],
          'sources': [
            'src/processor/microdump.cc',
            'src/processor/microdump_processor.cc',
            'src/processor/microdump_stackwalk.cc',
          ],
          'conditions': [
            ['OS=="ios"', {
              'toolsets': ['host'],
            }],
          ],
        },
        {
          # GN version: //breakpad:minidump_stackwalk
          'target_name': 'minidump_stackwalk',
          'type': 'executable',
          'dependencies': ['stackwalk_common'],
          'includes': ['breakpad_tools.gypi'],
          'defines': ['BPLOG_MINIMUM_SEVERITY=SEVERITY_ERROR'],
          'sources': [
            'src/processor/exploitability.cc',
            'src/processor/exploitability_linux.cc',
            'src/processor/exploitability_linux.h',
            'src/processor/exploitability_win.cc',
            'src/processor/exploitability_win.h',
            'src/processor/minidump.cc',
            'src/processor/minidump_processor.cc',
            'src/processor/minidump_stackwalk.cc',
            'src/processor/symbolic_constants_win.cc',
            'src/processor/symbolic_constants_win.h',
          ],
          'conditions': [
            ['OS=="ios"', {
              'toolsets': ['host'],
            }],
          ],
        },
        {
          # GN version: //breakpad:minidump_dump
          'target_name': 'minidump_dump',
          'type': 'executable',
          'includes': ['breakpad_tools.gypi'],
          'sources': [
            'src/processor/basic_code_module.h',
            'src/processor/basic_code_modules.cc',
            'src/processor/basic_code_modules.h',
            'src/processor/dump_context.cc',
            'src/processor/dump_object.cc',
            'src/processor/logging.cc',
            'src/processor/logging.h',
            'src/processor/minidump.cc',
            'src/processor/minidump_dump.cc',
            'src/processor/pathname_stripper.cc',
            'src/processor/pathname_stripper.h',
            'src/processor/proc_maps_linux.cc',
          ],
          'conditions': [
            ['OS=="ios"', {
              'toolsets': ['host'],
            }],
          ],
        },
      ],
    }],
    ['OS=="mac" or OS=="ios"', {
      'target_defaults': {
        'include_dirs': [
          'src',
        ],
        'configurations': {
          'Debug_Base': {
            'defines': [
              # This is needed for GTMLogger to work correctly.
              'DEBUG',
            ],
          },
        },
      },
      'targets': [
        {
          # GN version: //breakpad:dump_syms
          'target_name': 'dump_syms',
          'type': 'executable',
          'toolsets': ['host'],
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/dwarf/bytereader.cc',
            'src/common/dwarf_cfi_to_module.cc',
            'src/common/dwarf_cu_to_module.cc',
            'src/common/dwarf/dwarf2diehandler.cc',
            'src/common/dwarf/dwarf2reader.cc',
            'src/common/dwarf/elf_reader.cc',
            'src/common/dwarf/elf_reader.h',
            'src/common/dwarf_line_to_module.cc',
            'src/common/language.cc',
            'src/common/mac/arch_utilities.cc',
            'src/common/mac/arch_utilities.h',
            'src/common/mac/dump_syms.cc',
            'src/common/mac/file_id.cc',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_reader.cc',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_walker.cc',
            'src/common/md5.cc',
            'src/common/module.cc',
            'src/common/stabs_reader.cc',
            'src/common/stabs_to_module.cc',
            'src/tools/mac/dump_syms/dump_syms_tool.cc',
          ],
          'defines': [
            # For src/common/stabs_reader.h.
            'HAVE_MACH_O_NLIST_H',
          ],
          'xcode_settings': {
            # Like ld, dump_syms needs to operate on enough data that it may
            # actually need to be able to address more than 4GB. Use x86_64.
            # Don't worry! An x86_64 dump_syms is perfectly able to dump
            # 32-bit files.
            'ARCHS': [
              'x86_64',
            ],

            # The DWARF utilities require -funsigned-char.
            'GCC_CHAR_IS_UNSIGNED_CHAR': 'YES',

            # dwarf2reader.cc uses dynamic_cast.
            'GCC_ENABLE_CPP_RTTI': 'YES',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
          'configurations': {
            'Release_Base': {
              'xcode_settings': {
                # dump_syms crashes when built at -O1, -O2, and -O3.  It does
                # not crash at -Os.  To play it safe, dump_syms is always built
                # at -O0 until this can be sorted out.
                # http://code.google.com/p/google-breakpad/issues/detail?id=329
                'GCC_OPTIMIZATION_LEVEL': '0',  # -O0
               },
             },
          },
        },
        {
          # GN version: //breakpad:symupload
          'target_name': 'symupload',
          'type': 'executable',
          'toolsets': ['host'],
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/tools/mac/symupload/symupload.m',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
      ],
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'include_dirs': [
          'src',
        ],
        'configurations': {
          'Debug_Base': {
            'defines': [
              # This is needed for GTMLogger to work correctly.
              'DEBUG',
            ],
          },
        },
      },
      'targets': [
        {
          # GN version: //breakpad:utilities
          'target_name': 'breakpad_utilities',
          'type': 'static_library',
          'sources': [
            'src/client/mac/crash_generation/ConfigFile.mm',
            'src/client/mac/handler/breakpad_nlist_64.cc',
            'src/client/mac/handler/dynamic_images.cc',
            'src/client/mac/handler/minidump_generator.cc',
            'src/client/minidump_file_writer.cc',
            'src/common/convert_UTF.c',
            'src/common/mac/MachIPC.mm',
            'src/common/mac/arch_utilities.cc',
            'src/common/mac/bootstrap_compat.cc',
            'src/common/mac/file_id.cc',
            'src/common/mac/launch_reporter.cc',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_walker.cc',
            'src/common/mac/string_utilities.cc',
            'src/common/md5.cc',
            'src/common/simple_string_dictionary.cc',
            'src/common/string_conversion.cc',
          ],
          'conditions': [
            ['OS=="ios"', {
              'xcode_settings' : {
                'WARNING_CFLAGS': [
                  # See https://bugs.chromium.org/p/google-breakpad/issues/detail?id=675.
                  # TODO(crbug.com/569158): remove when fixed.
                  '-Wno-deprecated-declarations',
                ],
              },
            }],
          ],
        },
        {
          # GN version: //breakpad:crash_inspector
          'target_name': 'crash_inspector',
          'type': 'executable',
          'variables': {
            'mac_real_dsym': 1,
          },
          'dependencies': [
            'breakpad_utilities',
          ],
          'include_dirs': [
            'src/client/apple/Framework',
            'src/common/mac',
          ],
          'sources': [
            'src/client/mac/crash_generation/Inspector.mm',
            'src/client/mac/crash_generation/InspectorMain.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          # GN version: //breakpad:crash_report_sender
          'target_name': 'crash_report_sender',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'mac_real_dsym': 1,
          },
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/client/mac/sender/crash_report_sender.m',
            'src/client/mac/sender/uploader.mm',
            'src/common/mac/GTMLogger.m',
          ],
          'mac_bundle_resources': [
            'src/client/mac/sender/English.lproj/Localizable.strings',
            'src/client/mac/sender/crash_report_sender.icns',
            'src/client/mac/sender/Breakpad.xib',
            'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'mac_bundle_resources!': [
             'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'xcode_settings': {
             'INFOPLIST_FILE': 'src/client/mac/sender/crash_report_sender-Info.plist',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ],
          }
        },
        {
          # GN version: //breakpad
          'target_name': 'breakpad',
          'type': 'static_library',
          'dependencies': [
            'breakpad_utilities',
            'crash_inspector',
            'crash_report_sender',
          ],
          'include_dirs': [
            'src/client/apple/Framework',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src/client/apple/Framework',
            ],
          },
          'defines': [
            'USE_PROTECTED_ALLOCATIONS=1',
          ],
          'sources': [
            'src/client/mac/crash_generation/crash_generation_client.cc',
            'src/client/mac/crash_generation/crash_generation_client.h',
            'src/client/mac/handler/protected_memory_allocator.cc',
            'src/client/mac/handler/exception_handler.cc',
            'src/client/mac/Framework/Breakpad.mm',
            'src/client/mac/Framework/OnDemandServer.mm',
          ],
        },
      ],
    }],
    [ 'OS=="linux" or OS=="android" or os_bsd==1', {
      'conditions': [
        ['OS=="android"', {
          'defines': [
            '__ANDROID__',
          ],
        }],
      ],
      # Tools needed for archiving build symbols.
      'targets': [
        {
          # GN version: //breakpad:symupload
          'target_name': 'symupload',
          'type': 'executable',

          'includes': ['breakpad_tools.gypi'],

          'sources': [
            'src/tools/linux/symupload/sym_upload.cc',
            'src/common/linux/http_upload.cc',
            'src/common/linux/http_upload.h',
            'src/common/linux/symbol_upload.cc',
            'src/common/linux/symbol_upload.h',
          ],
          'include_dirs': [
            'src',
            'src/third_party',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        },
        {
          # GN version: //breakpad:dump_syms
          'target_name': 'dump_syms',
          'type': 'executable',
          'toolsets': ['host'],

          # dwarf2reader.cc uses dynamic_cast. Because we don't typically
          # don't support RTTI, we enable it for this single target. Since
          # dump_syms doesn't share any object files with anything else,
          # this doesn't end up polluting Chrome itself.
          'cflags_cc!': ['-fno-rtti'],

          'sources': [
            'src/common/dwarf/bytereader.cc',
            'src/common/dwarf_cfi_to_module.cc',
            'src/common/dwarf_cfi_to_module.h',
            'src/common/dwarf_cu_to_module.cc',
            'src/common/dwarf_cu_to_module.h',
            'src/common/dwarf/dwarf2diehandler.cc',
            'src/common/dwarf/dwarf2reader.cc',
            'src/common/dwarf/elf_reader.cc',
            'src/common/dwarf/elf_reader.h',
            'src/common/dwarf_line_to_module.cc',
            'src/common/dwarf_line_to_module.h',
            'src/common/language.cc',
            'src/common/language.h',
            'src/common/linux/crc32.cc',
            'src/common/linux/crc32.h',
            'src/common/linux/dump_symbols.cc',
            'src/common/linux/dump_symbols.h',
            'src/common/linux/elf_symbols_to_module.cc',
            'src/common/linux/elf_symbols_to_module.h',
            'src/common/linux/elfutils.cc',
            'src/common/linux/elfutils.h',
            'src/common/linux/file_id.cc',
            'src/common/linux/file_id.h',
            'src/common/linux/linux_libc_support.cc',
            'src/common/linux/linux_libc_support.h',
            'src/common/linux/memory_mapped_file.cc',
            'src/common/linux/memory_mapped_file.h',
            'src/common/linux/guid_creator.h',
            'src/common/module.cc',
            'src/common/module.h',
            'src/common/stabs_reader.cc',
            'src/common/stabs_reader.h',
            'src/common/stabs_to_module.cc',
            'src/common/stabs_to_module.h',
            'src/tools/linux/dump_syms/dump_syms.cc',
          ],

          # Breakpad rev 583 introduced this flag.
          # Using this define, stabs_reader.h will include a.out.h to
          # build on Linux.
          'defines': [
            'HAVE_A_OUT_H',
          ],

          'include_dirs': [
            'src',
            '..',
          ],
        },
        {
          # GN version: //breakpad:client
          'target_name': 'breakpad_client',
          'type': 'static_library',

          'sources': [
            'src/client/linux/crash_generation/crash_generation_client.cc',
            'src/client/linux/crash_generation/crash_generation_client.h',
            'src/client/linux/handler/exception_handler.cc',
            'src/client/linux/handler/exception_handler.h',
            'src/client/linux/handler/minidump_descriptor.cc',
            'src/client/linux/handler/minidump_descriptor.h',
            'src/client/linux/log/log.cc',
            'src/client/linux/log/log.h',
            'src/client/linux/dump_writer_common/mapping_info.h',
            'src/client/linux/dump_writer_common/thread_info.cc',
            'src/client/linux/dump_writer_common/thread_info.h',
            'src/client/linux/dump_writer_common/ucontext_reader.cc',
            'src/client/linux/dump_writer_common/ucontext_reader.h',
            'src/client/linux/microdump_writer/microdump_writer.cc',
            'src/client/linux/microdump_writer/microdump_writer.h',
            'src/client/linux/minidump_writer/cpu_set.h',
            'src/client/linux/minidump_writer/directory_reader.h',
            'src/client/linux/minidump_writer/line_reader.h',
            'src/client/linux/minidump_writer/linux_core_dumper.cc',
            'src/client/linux/minidump_writer/linux_core_dumper.h',
            'src/client/linux/minidump_writer/linux_dumper.cc',
            'src/client/linux/minidump_writer/linux_dumper.h',
            'src/client/linux/minidump_writer/linux_ptrace_dumper.cc',
            'src/client/linux/minidump_writer/linux_ptrace_dumper.h',
            'src/client/linux/minidump_writer/minidump_writer.cc',
            'src/client/linux/minidump_writer/minidump_writer.h',
            'src/client/linux/minidump_writer/proc_cpuinfo_reader.h',
            'src/client/minidump_file_writer-inl.h',
            'src/client/minidump_file_writer.cc',
            'src/client/minidump_file_writer.h',
            'src/common/convert_UTF.c',
            'src/common/convert_UTF.h',
            'src/common/linux/elf_core_dump.cc',
            'src/common/linux/elf_core_dump.h',
            'src/common/linux/elfutils.cc',
            'src/common/linux/elfutils.h',
            'src/common/linux/file_id.cc',
            'src/common/linux/file_id.h',
            'src/common/linux/google_crashdump_uploader.cc',
            'src/common/linux/google_crashdump_uploader.h',
            'src/common/linux/guid_creator.cc',
            'src/common/linux/guid_creator.h',
            'src/common/linux/libcurl_wrapper.cc',
            'src/common/linux/libcurl_wrapper.h',
            'src/common/linux/linux_libc_support.cc',
            'src/common/linux/linux_libc_support.h',
            'src/common/linux/memory_mapped_file.cc',
            'src/common/linux/memory_mapped_file.h',
            'src/common/linux/safe_readlink.cc',
            'src/common/linux/safe_readlink.h',
            'src/common/memory.h',
            'src/common/simple_string_dictionary.cc',
            'src/common/simple_string_dictionary.h',
            'src/common/string_conversion.cc',
            'src/common/string_conversion.h',
          ],

          'conditions': [
            ['target_arch=="arm" and chromeos==1', {
              # Avoid running out of registers in
              # linux_syscall_support.h:sys_clone()'s inline assembly.
              'cflags': ['-marm'],
            }],
            ['chromeos==1', {
              'defines': ['__CHROMEOS__'],
            }],
            ['OS=="android"', {
              'include_dirs': [
                'src/common/android/include',
              ],
              'direct_dependent_settings': {
                'include_dirs': [
                  'src/common/android/include',
                ],
              },
              'sources': [
                'src/common/android/breakpad_getcontext.S',
              ],
            }],
            ['OS!="android"', {
              'link_settings': {
                'libraries': [
                  # In case of Android, '-ldl' is added in common.gypi, since it
                  # is needed for stlport_static. For LD, the order of libraries
                  # is important, and thus we skip to add it here.
                  '-ldl',
                ],
              },
            }],
            ['clang==1 and target_arch=="ia32"', {
              'cflags!': [
                # Clang's -mstackrealign doesn't work well with
                # linux_syscall_support.h hand written asm syscalls.
                # See https://crbug.com/556393
                '-mstackrealign',
              ],
            }],
          ],

          'include_dirs': [
            'src',
            'src/client',
            'src/third_party/linux/include',
            '..',
            '.',
          ],
        },
        {
          # Breakpad r693 uses some files from src/processor in unit tests.
          # GN version: //breakpad:processor_support
          'target_name': 'breakpad_processor_support',
          'type': 'static_library',

          'sources': [
            'src/common/scoped_ptr.h',
            'src/processor/basic_code_modules.cc',
            'src/processor/basic_code_modules.h',
            'src/processor/dump_context.cc',
            'src/processor/dump_object.cc',
            'src/processor/logging.cc',
            'src/processor/logging.h',
            'src/processor/minidump.cc',
            'src/processor/pathname_stripper.cc',
            'src/processor/pathname_stripper.h',
            'src/processor/proc_maps_linux.cc',
          ],

          'include_dirs': [
            'src',
            'src/client',
            'src/third_party/linux/include',
            '..',
            '.',
          ],
        },
        {
          # GN version: //breakpad:breakpad_unittests
          'target_name': 'breakpad_unittests',
          'type': 'executable',
          'dependencies': [
            '../testing/gtest.gyp:gtest',
            '../testing/gtest.gyp:gtest_main',
            '../testing/gmock.gyp:gmock',
            'breakpad_client',
            'breakpad_processor_support',
            'linux_dumper_unittest_helper',
          ],
          'variables': {
            'clang_warning_flags': [
              # See http://crbug.com/138571#c18
              '-Wno-unused-value',
            ],
          },

          'sources': [
            'linux/breakpad_googletest_includes.h',
            'src/client/linux/handler/exception_handler_unittest.cc',
            'src/client/linux/minidump_writer/cpu_set_unittest.cc',
            'src/client/linux/minidump_writer/directory_reader_unittest.cc',
            'src/client/linux/minidump_writer/line_reader_unittest.cc',
            'src/client/linux/minidump_writer/linux_core_dumper_unittest.cc',
            'src/client/linux/minidump_writer/linux_ptrace_dumper_unittest.cc',
            'src/client/linux/minidump_writer/minidump_writer_unittest.cc',
            'src/client/linux/minidump_writer/minidump_writer_unittest_utils.cc',
            'src/client/linux/minidump_writer/proc_cpuinfo_reader_unittest.cc',
            'src/common/linux/elf_core_dump_unittest.cc',
            'src/common/linux/file_id_unittest.cc',
            'src/common/linux/linux_libc_support_unittest.cc',
            'src/common/linux/synth_elf.cc',
            'src/common/linux/tests/auto_testfile.h',
            'src/common/linux/tests/crash_generator.cc',
            'src/common/linux/tests/crash_generator.h',
            'src/common/memory_range.h',
            'src/common/memory_unittest.cc',
            'src/common/simple_string_dictionary_unittest.cc',
            'src/common/test_assembler.cc',
            'src/common/tests/file_utils.cc',
            'src/common/tests/file_utils.h',
            'src/tools/linux/md2core/minidump_memory_range.h',
            'src/tools/linux/md2core/minidump_memory_range_unittest.cc',
          ],

          # The build-id is required to test the minidump writer.
          'ldflags': [
            "-Wl,--build-id=0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
          ],

          'include_dirs': [
            'linux', # Use our copy of breakpad_googletest_includes.h
            'src',
            '..',
            '.',
          ],
          'conditions': [
            ['OS=="android"', {
              'libraries': [
                '-llog',
              ],
              'include_dirs': [
                'src/common/android/include',
              ],
              'sources': [
                'src/common/android/breakpad_getcontext_unittest.cc',
              ],
              'variables': {
                'test_type': 'gtest',
                'test_suite_name': '<(_target_name)',
              },
              'includes': [ '../build/android/test_runner.gypi' ],
              'ldflags!': [
                # We are overriding the build-id above so remove the default.
                '-Wl,--build-id=sha1',
              ],
            }],
            ['clang==1 and target_arch=="ia32"', {
              'cflags!': [
                # Clang's -mstackrealign doesn't work well with
                # linux_syscall_support.h hand written asm syscalls.
                # See https://crbug.com/556393
                '-mstackrealign',
              ],
            }],
          ],
        },
        {
          # GN version: //breakpad:linux_dumper_unittest_helper
          'target_name': 'linux_dumper_unittest_helper',
          'type': 'executable',
          'dependencies': [
            'breakpad_processor_support',
          ],
          'sources': [
            'src/client/linux/minidump_writer/linux_dumper_unittest_helper.cc',
          ],

          'include_dirs': [
            'src',
            '..',
          ],
          'conditions': [
            ['target_arch=="mipsel" and OS=="android"', {
              'include_dirs': [
                'src/common/android/include',
              ],
            }],
          ],
        },
        {
          # GN version: //breakpad:generate_test_dump
          'target_name': 'generate_test_dump',
          'type': 'executable',

          'sources': [
            'linux/generate-test-dump.cc',
          ],

          'dependencies': [
            'breakpad_client',
          ],

          'include_dirs': [
            '..',
            'src',
          ],
          'conditions': [
            ['OS=="android"', {
              'libraries': [
                '-llog',
              ],
              'include_dirs': [
                'src/common/android/include',
              ],
            }],
          ],
        },
        {
          # GN version: //breakpad:minidump-2-core
          'target_name': 'minidump-2-core',
          'type': 'executable',

          'sources': [
            'src/tools/linux/md2core/minidump-2-core.cc'
          ],

          'dependencies': [
            'breakpad_client',
          ],

          'include_dirs': [
            '..',
            'src',
          ],
        },
        {
          # GN version: //breakpad:core-2-minidump
          'target_name': 'core-2-minidump',
          'type': 'executable',

          'sources': [
            'src/tools/linux/core2md/core2md.cc'
          ],

          'dependencies': [
            'breakpad_client',
          ],

          'include_dirs': [
            '..',
            'src',
          ],
        },
      ],
    }],
    ['OS=="ios"', {
      'targets': [
        {
          # GN version: //breakpad:client
          'target_name': 'breakpad_client',
          'type': 'static_library',
          'sources': [
            'src/client/ios/Breakpad.h',
            'src/client/ios/Breakpad.mm',
            'src/client/ios/BreakpadController.h',
            'src/client/ios/BreakpadController.mm',
            'src/client/ios/handler/ios_exception_minidump_generator.mm',
            'src/client/ios/handler/ios_exception_minidump_generator.h',
            'src/client/mac/crash_generation/ConfigFile.h',
            'src/client/mac/crash_generation/ConfigFile.mm',
            'src/client/mac/handler/breakpad_nlist_64.cc',
            'src/client/mac/handler/breakpad_nlist_64.h',
            'src/client/mac/handler/dynamic_images.cc',
            'src/client/mac/handler/dynamic_images.h',
            'src/client/mac/handler/protected_memory_allocator.cc',
            'src/client/mac/handler/protected_memory_allocator.h',
            'src/client/mac/handler/exception_handler.cc',
            'src/client/mac/handler/exception_handler.h',
            'src/client/mac/handler/minidump_generator.cc',
            'src/client/mac/handler/minidump_generator.h',
            'src/client/mac/sender/uploader.h',
            'src/client/mac/sender/uploader.mm',
            'src/client/minidump_file_writer.cc',
            'src/client/minidump_file_writer.h',
            'src/client/minidump_file_writer-inl.h',
            'src/common/convert_UTF.c',
            'src/common/convert_UTF.h',
            'src/common/mac/file_id.cc',
            'src/common/mac/file_id.h',
            'src/common/mac/HTTPMultipartUpload.m',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_id.h',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_utilities.h',
            'src/common/mac/macho_walker.cc',
            'src/common/mac/macho_walker.h',
            'src/common/mac/string_utilities.cc',
            'src/common/mac/string_utilities.h',
            'src/common/md5.cc',
            'src/common/md5.h',
            'src/common/simple_string_dictionary.cc',
            'src/common/simple_string_dictionary.h',
            'src/common/string_conversion.cc',
            'src/common/string_conversion.h',
            'src/google_breakpad/common/minidump_format.h',
          ],
          'include_dirs': [
            'src',
            'src/client/mac/Framework',
            'src/common/mac',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
          'xcode_settings' : {
            'WARNING_CFLAGS': [
              # See https://bugs.chromium.org/p/google-breakpad/issues/detail?id=675.
              # TODO(crbug.com/569158): remove when fixed.
              '-Wno-deprecated-declarations',
            ],
          },
        }
      ]
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'breakpad_unittests_stripped',
          'type': 'none',
          'dependencies': [ 'breakpad_unittests' ],
          'actions': [{
            'action_name': 'strip breakpad_unittests',
            'inputs': [ '<(PRODUCT_DIR)/breakpad_unittests' ],
            'outputs': [ '<(PRODUCT_DIR)/breakpad_unittests_stripped' ],
            'action': [ '<(android_strip)', '<@(_inputs)', '-o', '<@(_outputs)' ],
          }],
        },
        {
          'target_name': 'breakpad_unittests_deps',
          'type': 'none',
          'dependencies': [
            'breakpad_unittests',
            'linux_dumper_unittest_helper',
          ],
          'variables': {
             'output_dir': '<(PRODUCT_DIR)/breakpad_unittests__dist/',
             'native_binary': '<(PRODUCT_DIR)/breakpad_unittests',
             'include_main_binary': 1,
             'extra_files': ['<(PRODUCT_DIR)/linux_dumper_unittest_helper'],
          },
          'includes': [
            '../build/android/native_app_dependencies.gypi'
          ],
        }
      ],
      'conditions': [
        ['test_isolation_mode != "noop"',
          {
            'targets': [
              {
                'target_name': 'breakpad_unittests_apk_run',
                'type': 'none',
                'dependencies': [
                  'breakpad_unittests',
                ],
                'includes': [
                  '../build/isolate.gypi',
                ],
                'sources': [
                  'breakpad_unittests_apk.isolate',
                ],
              },
            ]
          }
        ],
      ],
    }],
  ],
}
