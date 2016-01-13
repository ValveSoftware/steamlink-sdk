// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/create_file/chrome_create_file.h"

#include <windows.h>

#include <bitset>
#include <string>

#include "base/base_paths_win.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/ntdll_cache.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/nt_internals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"


namespace {

// Test fixtures -------------------------------------------------------------

class ChromeCreateFileTest : public PlatformTest {
 protected:
  struct NtCreateFileParams {
    ACCESS_MASK desired_access;
    OBJECT_ATTRIBUTES object_attributes;
    PLARGE_INTEGER allocation_size;
    ULONG file_attributes;
    ULONG share_access;
    ULONG create_disposition;
    ULONG create_options;
    PVOID ea_buffer;
    ULONG ea_length;
  };

  enum CallPath {
    ELF,
    KERNEL
  };

  template<CallPath path>
  static NTSTATUS WINAPI FakeNtCreateFile(
      PHANDLE file_handle,
      ACCESS_MASK desired_access,
      POBJECT_ATTRIBUTES object_attributes,
      PIO_STATUS_BLOCK io_status_block,
      PLARGE_INTEGER allocation_size,
      ULONG file_attributes,
      ULONG share_access,
      ULONG create_disposition,
      ULONG create_options,
      PVOID ea_buffer,
      ULONG ea_length) {
    return self_->HandleCreateFileCall(file_handle,
        desired_access,
        object_attributes,
        io_status_block,
        allocation_size,
        file_attributes,
        share_access,
        create_disposition,
        create_options,
        ea_buffer,
        ea_length,
        path);
  }

  virtual void SetUp() OVERRIDE {
    original_thread_ = base::PlatformThread::CurrentId();
    InitCache();
    PlatformTest::SetUp();

    base::FilePath user_data_dir;
    PathService::Get(base::DIR_LOCAL_APP_DATA, &user_data_dir);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(user_data_dir));
    ASSERT_TRUE(temp_dir2_.CreateUniqueTempDir());
    self_ = this;
  }

  void UnsetThunkStorage() {
    DWORD old_protect = 0;
    EXPECT_TRUE(::VirtualProtect(&g_nt_thunk_storage,
                                 sizeof(g_nt_thunk_storage),
                                 PAGE_EXECUTE_READWRITE,
                                 &old_protect));
    memset(&g_nt_thunk_storage, 0, sizeof(g_nt_thunk_storage));

    EXPECT_TRUE(::VirtualProtect(&g_nt_thunk_storage,
                                 sizeof(g_nt_thunk_storage),
                                 PAGE_EXECUTE_READ,
                                 &old_protect));
  }

  void RedirectNtCreateFileCalls() {
    UnsetThunkStorage();
    old_func_ptr_ =
        reinterpret_cast<NtCreateFileFunction>(g_ntdll_lookup["NtCreateFile"]);

    // KernelBase.dll only exists for Win7 and later, prior to that, kernel32
    // imports from ntdll directly.
    if (base::win::GetVersion() < base::win::VERSION_WIN7) {
      patcher_.Patch(L"kernel32.dll", "ntdll.dll", "NtCreateFile",
          reinterpret_cast<void(*)()>(&FakeNtCreateFile<KERNEL>));
    } else {
      patcher_.Patch(L"kernelbase.dll", "ntdll.dll", "NtCreateFile",
          reinterpret_cast<void(*)()>(&FakeNtCreateFile<KERNEL>));
    }

    g_ntdll_lookup["NtCreateFile"] = reinterpret_cast<void(*)()>(
        &ChromeCreateFileTest::FakeNtCreateFile<ELF>);
  }

  void ResetNtCreateFileCalls() {
    g_ntdll_lookup["NtCreateFile"] = reinterpret_cast<void*>(old_func_ptr_);
    patcher_.Unpatch();
  }

  NTSTATUS HandleCreateFileCall(PHANDLE file_handle,
      ACCESS_MASK desired_access,
      POBJECT_ATTRIBUTES object_attributes,
      PIO_STATUS_BLOCK io_status_block,
      PLARGE_INTEGER allocation_size,
      ULONG file_attributes,
      ULONG share_access,
      ULONG create_disposition,
      ULONG create_options,
      PVOID ea_buffer,
      ULONG ea_length,
      CallPath call_path) {
    if (original_thread_ == base::PlatformThread::CurrentId()) {
      SetParams(desired_access,
          object_attributes,
          allocation_size,
          file_attributes,
          share_access,
          create_disposition,
          create_options,
          ea_buffer,
          ea_length,
          call_path == ELF ? &elf_params_ : &kernel_params_);
    }

    // Forward the call to the real NTCreateFile.
    return old_func_ptr_(file_handle,
        desired_access,
        object_attributes,
        io_status_block,
        allocation_size,
        file_attributes,
        share_access,
        create_disposition,
        create_options,
        ea_buffer,
        ea_length);
  }

  void SetParams(ACCESS_MASK desired_access,
      POBJECT_ATTRIBUTES object_attributes,
      PLARGE_INTEGER allocation_size,
      ULONG file_attributes,
      ULONG share_access,
      ULONG create_disposition,
      ULONG create_options,
      PVOID ea_buffer,
      ULONG ea_length,
      NtCreateFileParams* params) {
    params->desired_access = desired_access;
    params->object_attributes.Length = object_attributes->Length;
    params->object_attributes.ObjectName = object_attributes->ObjectName;
    params->object_attributes.RootDirectory = object_attributes->RootDirectory;
    params->object_attributes.Attributes = object_attributes->Attributes;
    params->object_attributes.SecurityDescriptor =
        object_attributes->SecurityDescriptor;
    params->object_attributes.SecurityQualityOfService =
        object_attributes->SecurityQualityOfService;
    params->allocation_size = allocation_size;
    params->file_attributes = file_attributes;
    params->share_access = share_access;
    params->create_disposition = create_disposition;
    params->create_options = create_options;
    params->ea_buffer = ea_buffer;
    params->ea_length = ea_length;
  }

  void CheckParams() {
    std::bitset<32> elf((int) elf_params_.desired_access);
    std::bitset<32> ker((int) kernel_params_.desired_access);

    EXPECT_EQ(kernel_params_.desired_access, elf_params_.desired_access)
        << elf << "\n" << ker;
    EXPECT_EQ(kernel_params_.object_attributes.Length,
              elf_params_.object_attributes.Length);
    EXPECT_EQ(kernel_params_.object_attributes.RootDirectory,
              elf_params_.object_attributes.RootDirectory);
    EXPECT_EQ(kernel_params_.object_attributes.Attributes,
              elf_params_.object_attributes.Attributes);
    EXPECT_EQ(kernel_params_.object_attributes.SecurityDescriptor,
              elf_params_.object_attributes.SecurityDescriptor);
    EXPECT_EQ(kernel_params_.allocation_size, elf_params_.allocation_size);
    EXPECT_EQ(kernel_params_.file_attributes, elf_params_.file_attributes);
    EXPECT_EQ(kernel_params_.share_access, elf_params_.share_access);
    EXPECT_EQ(kernel_params_.create_disposition,
              elf_params_.create_disposition);
    EXPECT_EQ(kernel_params_.create_options, elf_params_.create_options);
    EXPECT_EQ(kernel_params_.ea_buffer, elf_params_.ea_buffer);
    EXPECT_EQ(kernel_params_.ea_length, elf_params_.ea_length);
  }

  void DoWriteCheck(const base::FilePath& path, DWORD flag, bool is_system) {
    base::win::ScopedHandle file_handle;
    const char kTestData[] = "0123456789";
    int buffer_size = sizeof(kTestData) - 1;
    DWORD bytes_written;

    if (is_system) {
      file_handle.Set(::CreateFileW(path.value().c_str(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | flag,
                                    NULL));
    } else {
      file_handle.Set(CreateFileNTDLL(path.value().c_str(),
                                      GENERIC_WRITE,
                                      FILE_SHARE_READ,
                                      NULL,
                                      CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL | flag,
                                      NULL));
    }


    EXPECT_FALSE(file_handle == INVALID_HANDLE_VALUE);
    ::WriteFile(file_handle, kTestData, buffer_size, &bytes_written, NULL);
    EXPECT_EQ(buffer_size, bytes_written);
  }

  void DoReadCheck(const base::FilePath& path, DWORD flag, bool is_system) {
    base::win::ScopedHandle file_handle;
    const char kTestData[] = "0123456789";
    int buffer_size = sizeof(kTestData) - 1;
    DWORD bytes_read;
    char read_buffer[10];

    if (is_system) {
      file_handle.Set(::CreateFileW(path.value().c_str(),
                                    GENERIC_READ,
                                    0,
                                    NULL,
                                    OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | flag,
                                    NULL));
    } else {
      file_handle.Set(CreateFileNTDLL(path.value().c_str(),
                                      GENERIC_READ,
                                      0,
                                      NULL,
                                      OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL | flag,
                                      NULL));
    }

    EXPECT_FALSE(file_handle == INVALID_HANDLE_VALUE);
    ::ReadFile(file_handle, read_buffer, buffer_size, &bytes_read, NULL);
    EXPECT_EQ(buffer_size, bytes_read);
    EXPECT_EQ(0, memcmp(kTestData, read_buffer, bytes_read));
  }

  void RunChecks(DWORD flag, bool check_reads) {
    // Make sure we can write to this file handle when called via the system.
    base::FilePath junk_path_1 = temp_dir_.path().Append(L"junk_1.txt");
    base::FilePath junk_path_2 = temp_dir_.path().Append(L"junk_2.txt");
    DoWriteCheck(junk_path_1, flag, true);
    DoWriteCheck(junk_path_2, flag, false);
    CheckParams();

    if (check_reads) {
      // Make sure we can read from this file handle when called via the system.
      DoReadCheck(junk_path_1, flag, true);
      DoReadCheck(junk_path_2, flag, false);
      CheckParams();
    }
    base::DeleteFile(junk_path_1, false);
    base::DeleteFile(junk_path_2, false);

  }

  static ChromeCreateFileTest* self_;

  NtCreateFileFunction old_func_ptr_;
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir temp_dir2_;
  base::win::IATPatchFunction patcher_;
  NtCreateFileParams kernel_params_;
  NtCreateFileParams elf_params_;
  base::PlatformThreadId original_thread_;
};

ChromeCreateFileTest* ChromeCreateFileTest::self_ = NULL;

// Tests ---------------------------------------------------------------------
TEST_F(ChromeCreateFileTest, CheckParams_FILE_ATTRIBUTE_NORMAL) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_ATTRIBUTE_NORMAL, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_WRITE_THROUGH) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_WRITE_THROUGH, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_RANDOM_ACCESS) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_RANDOM_ACCESS, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_SEQUENTIAL_SCAN) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_SEQUENTIAL_SCAN, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_DELETE_ON_CLOSE) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_DELETE_ON_CLOSE, false);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_BACKUP_SEMANTICS) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_BACKUP_SEMANTICS, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_OPEN_REPARSE_POINT) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_OPEN_REPARSE_POINT, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, CheckParams_FILE_FLAG_OPEN_NO_RECALL) {
  RedirectNtCreateFileCalls();
  RunChecks(FILE_FLAG_OPEN_NO_RECALL, true);
  ResetNtCreateFileCalls();
}

TEST_F(ChromeCreateFileTest, BypassTest) {
  std::wstring UNC_filepath_file(L"\\\\.\\some_file.txt");

  base::FilePath local_path;
  PathService::Get(base::DIR_LOCAL_APP_DATA, &local_path);

  base::FilePath local_prefs_path = local_path.Append(kAppDataDirName).Append(
      kUserDataDirName).Append(L"default\\Preferences");
  base::FilePath local_state_path = local_path.Append(kAppDataDirName).Append(
      kUserDataDirName).Append(L"ninja\\Local State");
  base::FilePath local_junk_path = local_path.Append(kAppDataDirName).Append(
      kUserDataDirName).Append(L"default\\Junk");

  base::FilePath desktop_path;
  PathService::Get(base::DIR_USER_DESKTOP, &desktop_path);
  base::FilePath desktop_junk_path =
      desktop_path.Append(L"Downloads\\junk.txt");
  base::FilePath desktop_prefs_path =
      desktop_path.Append(L"Downloads\\Preferences");

  // Don't redirect UNC files.
  EXPECT_FALSE(ShouldBypass(UNC_filepath_file.c_str()));

  // Don't redirect if file is not in UserData directory.
  EXPECT_FALSE(ShouldBypass(desktop_junk_path.value().c_str()));
  EXPECT_FALSE(ShouldBypass(desktop_prefs_path.value().c_str()));

  // Only redirect "Preferences" and "Local State" files.
  EXPECT_TRUE(ShouldBypass(local_prefs_path.value().c_str()));
  EXPECT_TRUE(ShouldBypass(local_state_path.value().c_str()));
  EXPECT_FALSE(ShouldBypass(local_junk_path.value().c_str()));
}

TEST_F(ChromeCreateFileTest, ReadWriteFromNtDll) {
  UnsetThunkStorage();
  base::FilePath file_name = temp_dir_.path().Append(L"some_file.txt");
  DoWriteCheck(file_name, FILE_ATTRIBUTE_NORMAL, false);
  DoReadCheck(file_name, FILE_ATTRIBUTE_NORMAL, false);
}

TEST_F(ChromeCreateFileTest, ReadWriteFromThunk) {
  base::FilePath file_name = temp_dir_.path().Append(L"some_file.txt");
  DoWriteCheck(file_name, FILE_ATTRIBUTE_NORMAL, false);
  DoReadCheck(file_name, FILE_ATTRIBUTE_NORMAL, false);
}

}  // namespace
