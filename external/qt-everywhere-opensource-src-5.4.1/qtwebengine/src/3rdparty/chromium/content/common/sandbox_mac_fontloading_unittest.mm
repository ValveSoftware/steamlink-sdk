// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "content/common/mac/font_descriptor.h"
#include "content/common/mac/font_loader.h"
#include "content/common/sandbox_mac_unittest_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FontLoadingTestCase : public MacSandboxTestCase {
 public:
  FontLoadingTestCase() : font_data_length_(-1) {}
  virtual bool BeforeSandboxInit() OVERRIDE;
  virtual bool SandboxedTest() OVERRIDE;
 private:
  scoped_ptr<base::SharedMemory> font_shmem_;
  size_t font_data_length_;
};
REGISTER_SANDBOX_TEST_CASE(FontLoadingTestCase);


// Load raw font data into shared memory object.
bool FontLoadingTestCase::BeforeSandboxInit() {
  std::string font_data;
  if (!base::ReadFileToString(base::FilePath(test_data_.c_str()), &font_data)) {
    LOG(ERROR) << "Failed to read font data from file (" << test_data_ << ")";
    return false;
  }

  font_data_length_ = font_data.length();
  if (font_data_length_ <= 0) {
    LOG(ERROR) << "No font data: " << font_data_length_;
    return false;
  }

  font_shmem_.reset(new base::SharedMemory);
  if (!font_shmem_) {
    LOG(ERROR) << "Failed to create shared memory object.";
    return false;
  }

  if (!font_shmem_->CreateAndMapAnonymous(font_data_length_)) {
    LOG(ERROR) << "SharedMemory::Create failed";
    return false;
  }

  memcpy(font_shmem_->memory(), font_data.c_str(), font_data_length_);
  if (!font_shmem_->Unmap())  {
    LOG(ERROR) << "SharedMemory::Unmap failed";
    return false;
  }
  return true;
}

bool FontLoadingTestCase::SandboxedTest() {
  base::SharedMemoryHandle shmem_handle;
  if (!font_shmem_->ShareToProcess(base::kNullProcessHandle, &shmem_handle)) {
    LOG(ERROR) << "SharedMemory::ShareToProcess failed";
    return false;
  }

  CGFontRef cg_font_ref;
  if (!FontLoader::CGFontRefFromBuffer(shmem_handle, font_data_length_,
                                       &cg_font_ref)) {
    LOG(ERROR) << "Call to CreateCGFontFromBuffer() failed";
    return false;
  }

  if (!cg_font_ref) {
    LOG(ERROR) << "Got NULL CGFontRef";
    return false;
  }
  base::ScopedCFTypeRef<CGFontRef> cgfont(cg_font_ref);

  CTFontRef ct_font_ref =
      CTFontCreateWithGraphicsFont(cgfont.get(), 16.0, NULL, NULL);
  base::ScopedCFTypeRef<CTFontRef> ctfont(ct_font_ref);

  if (!ct_font_ref) {
    LOG(ERROR) << "CTFontCreateWithGraphicsFont() failed";
    return false;
  }

  // Do something with the font to make sure it's loaded.
  CGFloat cap_height = CTFontGetCapHeight(ct_font_ref);

  if (cap_height <= 0.0) {
    LOG(ERROR) << "Got bad value for CTFontGetCapHeight " << cap_height;
    return false;
  }

  return true;
}

TEST_F(MacSandboxTest, FontLoadingTest) {
  base::FilePath temp_file_path;
  FILE* temp_file = base::CreateAndOpenTemporaryFile(&temp_file_path);
  ASSERT_TRUE(temp_file);
  base::ScopedFILE temp_file_closer(temp_file);

  NSFont* srcFont = [NSFont fontWithName:@"Geeza Pro" size:16.0];
  FontDescriptor descriptor(srcFont);
  FontLoader::Result result;
  FontLoader::LoadFont(descriptor, &result);
  EXPECT_GT(result.font_data_size, 0U);
  EXPECT_GT(result.font_id, 0U);

  base::WriteFileDescriptor(fileno(temp_file),
      static_cast<const char *>(result.font_data.memory()),
      result.font_data_size);

  ASSERT_TRUE(RunTestInSandbox(SANDBOX_TYPE_RENDERER,
                  "FontLoadingTestCase", temp_file_path.value().c_str()));
  temp_file_closer.reset();
  ASSERT_TRUE(base::DeleteFile(temp_file_path, false));
}

}  // namespace content
