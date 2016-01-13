// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/sandbox_mac.h"
#include "content/common/sandbox_mac_unittest_helper.h"
#include "crypto/nss_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

//--------------------- Clipboard Sandboxing ----------------------
// Test case for checking sandboxing of clipboard access.
class MacSandboxedClipboardTestCase : public MacSandboxTestCase {
 public:
  MacSandboxedClipboardTestCase();
  virtual ~MacSandboxedClipboardTestCase();

  virtual bool SandboxedTest() OVERRIDE;

  virtual void SetTestData(const char* test_data) OVERRIDE;
 private:
  NSString* clipboard_name_;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedClipboardTestCase);

MacSandboxedClipboardTestCase::MacSandboxedClipboardTestCase() :
    clipboard_name_(nil) {}

MacSandboxedClipboardTestCase::~MacSandboxedClipboardTestCase() {
  [clipboard_name_ release];
}

bool MacSandboxedClipboardTestCase::SandboxedTest() {
  // Shouldn't be able to open the pasteboard in the sandbox.

  if ([clipboard_name_ length] == 0) {
    LOG(ERROR) << "Clipboard name is empty";
    return false;
  }

  NSPasteboard* pb = [NSPasteboard pasteboardWithName:clipboard_name_];
  if (pb != nil) {
    LOG(ERROR) << "Was able to access named clipboard";
    return false;
  }

  pb = [NSPasteboard generalPasteboard];
  if (pb != nil) {
    LOG(ERROR) << "Was able to access system clipboard";
    return false;
  }

  return true;
}

void MacSandboxedClipboardTestCase::SetTestData(const char* test_data) {
  clipboard_name_ = [base::SysUTF8ToNSString(test_data) retain];
}

TEST_F(MacSandboxTest, ClipboardAccess) {
  NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];
  EXPECT_EQ([[pb types] count], 0U);

  std::string pasteboard_name = base::SysNSStringToUTF8([pb name]);
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedClipboardTestCase",
                                       pasteboard_name.c_str()));

  // After executing the test, the clipboard should still be empty.
  EXPECT_EQ([[pb types] count], 0U);
}

//--------------------- File Access Sandboxing ----------------------
// Test case for checking sandboxing of filesystem apis.
class MacSandboxedFileAccessTestCase : public MacSandboxTestCase {
 public:
  virtual bool SandboxedTest() OVERRIDE;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedFileAccessTestCase);

bool MacSandboxedFileAccessTestCase::SandboxedTest() {
  base::ScopedFD fdes(HANDLE_EINTR(open("/etc/passwd", O_RDONLY)));
  return !fdes.is_valid();
}

TEST_F(MacSandboxTest, FileAccess) {
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedFileAccessTestCase", NULL));
}

//--------------------- /dev/urandom Sandboxing ----------------------
// /dev/urandom is available to any sandboxed process.
class MacSandboxedUrandomTestCase : public MacSandboxTestCase {
 public:
  virtual bool SandboxedTest() OVERRIDE;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedUrandomTestCase);

bool MacSandboxedUrandomTestCase::SandboxedTest() {
  base::ScopedFD fdes(HANDLE_EINTR(open("/dev/urandom", O_RDONLY)));

  // Opening /dev/urandom succeeds under the sandbox.
  if (!fdes.is_valid())
    return false;

  char buf[16];
  int rc = HANDLE_EINTR(read(fdes.get(), buf, sizeof(buf)));
  return rc == sizeof(buf);
}

TEST_F(MacSandboxTest, UrandomAccess) {
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedUrandomTestCase", NULL));
}

//--------------------- NSS Sandboxing ----------------------
// Test case for checking sandboxing of NSS initialization.
class MacSandboxedNSSTestCase : public MacSandboxTestCase {
 public:
  virtual bool SandboxedTest() OVERRIDE;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedNSSTestCase);

bool MacSandboxedNSSTestCase::SandboxedTest() {
  // If NSS cannot read from /dev/urandom, NSS initialization will call abort(),
  // which will cause this test case to fail.
  crypto::ForceNSSNoDBInit();
  crypto::EnsureNSSInit();
  return true;
}

TEST_F(MacSandboxTest, NSSAccess) {
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedNSSTestCase", NULL));
}

}  // namespace content
