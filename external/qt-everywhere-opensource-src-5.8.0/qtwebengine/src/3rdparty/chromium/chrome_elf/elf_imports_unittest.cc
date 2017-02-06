// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/win/pe_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ELFImportsTest : public testing::Test {
 protected:
  static bool ImportsCallback(const base::win::PEImage &image,
                              LPCSTR module,
                              PIMAGE_THUNK_DATA name_table,
                              PIMAGE_THUNK_DATA iat,
                              PVOID cookie) {
    std::vector<std::string>* import_list =
        reinterpret_cast<std::vector<std::string>*>(cookie);
    import_list->push_back(module);
    return true;
  }

  void GetImports(const base::FilePath& module_path,
                  std::vector<std::string>* imports) {
    ASSERT_TRUE(imports != NULL);

    base::MemoryMappedFile module_mmap;

    ASSERT_TRUE(module_mmap.Initialize(module_path));
    base::win::PEImageAsData pe_image_data(
        reinterpret_cast<HMODULE>(const_cast<uint8_t*>(module_mmap.data())));
    pe_image_data.EnumImportChunks(ELFImportsTest::ImportsCallback, imports);
  }
};

// Run this test only in Release builds.
//
// This test makes sure that chrome_elf.dll has only certain types of imports.
// However, it directly and indirectly depends on base, which has lots more
// imports than are allowed here.
//
// In release builds, the offending imports are all stripped since this
// depends on a relatively small portion of base. In GYP, this works in debug
// builds as well because static libraries are used for the sandbox and base
// targets and the files that use e.g. user32.dll happen to not get brought
// into the build in the first place (due to the way static libraries are
// linked where only the required .o files are included). But we don't bother
// differentiating GYP and GN builds for this purpose.
//
// If you break this test, you may have changed base or the Windows sandbox
// such that more system imports are required to link.
#if defined(NDEBUG) && !defined(COMPONENT_BUILD)
TEST_F(ELFImportsTest, ChromeElfSanityCheck) {
  base::FilePath dll;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");

  std::vector<std::string> elf_imports;
  GetImports(dll, &elf_imports);

  // Check that ELF has imports.
  ASSERT_LT(0u, elf_imports.size()) << "Ensure the chrome_elf_unittests "
    "target was built, instead of chrome_elf_unittests.exe";

  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
#if defined(COMPONENT_BUILD)
    "MSVC*.dll",
    "VCRUNTIME*.dll",
    "api-ms-win-crt-*.dll",
#endif
#if defined(SYZYASAN)
    "syzyasan_rtl.dll",
#endif
#if defined(ADDRESS_SANITIZER) && defined(COMPONENT_BUILD)
    "clang_rt.asan_dynamic-i386.dll",
#endif
    "ADVAPI32.dll",
    // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
    // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
    // be ok.
    "VERSION.dll",
  };

  // Make sure all of ELF's imports are in the valid imports list.
  for (const std::string& import : elf_imports) {
    bool match = false;
    for (const char* kValidFilePattern : kValidFilePatterns) {
      if (base::MatchPattern(import, kValidFilePattern)) {
        match = true;
        break;
      }
    }
    ASSERT_TRUE(match) << "Illegal import in chrome_elf.dll: " << import;
  }
}
TEST_F(ELFImportsTest, ChromeElfLoadSanityTest) {
  base::FilePath dll;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");

  // We don't expect user32 to be loaded in chrome_elf_unittests. If this test
  // case fails, then it means that a dependency on user32 has crept into the
  // chrome_elf_unittests executable, which needs to be removed.
  EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));

  HMODULE chrome_elf_module_handle = ::LoadLibrary(dll.value().c_str());
  EXPECT_TRUE(chrome_elf_module_handle != nullptr);
  // Loading chrome_elf.dll should not load user32.dll
  EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
  EXPECT_TRUE(!!::FreeLibrary(chrome_elf_module_handle));
}
#endif  // NDEBUG

TEST_F(ELFImportsTest, ChromeExeSanityCheck) {
  std::vector<std::string> exe_imports;

  base::FilePath exe;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &exe));
  exe = exe.Append(L"chrome.exe");
  GetImports(exe, &exe_imports);

  // Check that chrome.exe has imports.
  ASSERT_LT(0u, exe_imports.size()) << "Ensure the chrome_elf_unittests "
    "target was built, instead of chrome_elf_unittests.exe";

  // Chrome.exe's first import must be ELF.
  EXPECT_EQ("chrome_elf.dll", exe_imports[0]) <<
      "Illegal import order in chrome.exe (ensure the chrome_elf_unittest "
      "target was built, instead of just chrome_elf_unittests.exe)";
}

}  // namespace
