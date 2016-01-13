// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
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
        reinterpret_cast<HMODULE>(const_cast<uint8*>(module_mmap.data())));
    pe_image_data.EnumImportChunks(ELFImportsTest::ImportsCallback, imports);
  }
};

TEST_F(ELFImportsTest, ChromeElfSanityCheck) {
  std::vector<std::string> elf_imports;

  base::FilePath dll;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");
  GetImports(dll, &elf_imports);

  // Check that ELF has imports.
  ASSERT_LT(0u, elf_imports.size()) << "Ensure the chrome_elf_unittests "
    "target was built, instead of chrome_elf_unittests.exe";

  std::vector<std::string>::iterator it(elf_imports.begin());

  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
    "MSVC*",
#if defined(SYZYASAN)
    "syzyasan_rtl.dll",
#endif
    "ADVAPI32.dll"
  };

  // Make sure all of ELF's imports are in the valid imports list.
  for (; it != elf_imports.end(); it++) {
    bool match = false;
    for (int i = 0; i < arraysize(kValidFilePatterns); ++i) {
      if (MatchPattern(*it, kValidFilePatterns[i]))
        match = true;
    }
    ASSERT_TRUE(match) << "Illegal import in chrome_elf.dll.";
  }
}

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
