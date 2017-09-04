// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_message_filter.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/test/test_clipboard.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class ClipboardMessageFilterTest : public ::testing::Test {
 protected:
  ClipboardMessageFilterTest()
      : filter_(new ClipboardMessageFilter(nullptr)),
        clipboard_(ui::TestClipboard::CreateForCurrentThread()) {
    filter_->set_peer_process_for_testing(base::Process::Current());
  }

  ~ClipboardMessageFilterTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  std::unique_ptr<base::SharedMemory> CreateAndMapReadOnlySharedMemory(
      size_t size) {
    std::unique_ptr<base::SharedMemory> m = CreateReadOnlySharedMemory(size);
    if (!m->Map(size))
      return nullptr;
    return m;
  }

  std::unique_ptr<base::SharedMemory> CreateReadOnlySharedMemory(size_t size) {
    std::unique_ptr<base::SharedMemory> m(new base::SharedMemory());
    base::SharedMemoryCreateOptions options;
    options.size = size;
    options.share_read_only = true;
    if (!m->Create(options))
      return nullptr;
    return m;
  }

  void CallWriteImage(const gfx::Size& size,
                      base::SharedMemory* shared_memory) {
    base::SharedMemoryHandle handle;
    ASSERT_TRUE(shared_memory->GiveReadOnlyToProcess(
        base::GetCurrentProcessHandle(), &handle));
    CallWriteImageDirectly(size, handle);
  }

  // Prefer to use CallWriteImage() in tests.
  void CallWriteImageDirectly(const gfx::Size& size,
                              base::SharedMemoryHandle handle) {
    filter_->OnWriteImage(ui::CLIPBOARD_TYPE_COPY_PASTE, size, handle);
  }

  void CallCommitWrite() {
    filter_->OnCommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
    base::RunLoop().RunUntilIdle();
  }

  ui::Clipboard* clipboard() { return clipboard_; }

 private:
  const TestBrowserThreadBundle thread_bundle_;
  const scoped_refptr<ClipboardMessageFilter> filter_;
  ui::Clipboard* const clipboard_;
};

// Test that it actually works.
TEST_F(ClipboardMessageFilterTest, SimpleImage) {
  static const uint32_t bitmap_data[] = {
      0x33333333, 0xdddddddd, 0xeeeeeeee, 0x00000000,
      0x88888888, 0x66666666, 0x55555555, 0xbbbbbbbb,
      0x44444444, 0xaaaaaaaa, 0x99999999, 0x77777777,
      0xffffffff, 0x11111111, 0x22222222, 0xcccccccc,
  };

  std::unique_ptr<base::SharedMemory> shared_memory =
      CreateAndMapReadOnlySharedMemory(sizeof(bitmap_data));
  memcpy(shared_memory->memory(), bitmap_data, sizeof(bitmap_data));

  CallWriteImage(gfx::Size(4, 4), shared_memory.get());
  uint64_t sequence_number =
      clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE);
  CallCommitWrite();

  EXPECT_NE(sequence_number,
            clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ui::Clipboard::GetBitmapFormatType(), ui::CLIPBOARD_TYPE_COPY_PASTE));

  SkBitmap actual = clipboard()->ReadImage(ui::CLIPBOARD_TYPE_COPY_PASTE);
  SkAutoLockPixels locked(actual);
  EXPECT_EQ(sizeof(bitmap_data), actual.getSize());
  EXPECT_EQ(0,
            memcmp(bitmap_data, actual.getAddr32(0, 0), sizeof(bitmap_data)));
}

// Test with a size that would overflow a naive 32-bit row bytes calculation.
TEST_F(ClipboardMessageFilterTest, ImageSizeOverflows32BitRowBytes) {
  std::unique_ptr<base::SharedMemory> shared_memory =
      CreateReadOnlySharedMemory(0x20000000);

  CallWriteImage(gfx::Size(0x20000000, 1), shared_memory.get());
  uint64_t sequence_number =
      clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE);
  CallCommitWrite();

  EXPECT_EQ(sequence_number,
            clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardMessageFilterTest, InvalidSharedMemoryHandle) {
  CallWriteImageDirectly(gfx::Size(5, 5), base::SharedMemory::NULLHandle());
  uint64_t sequence_number =
      clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE);
  CallCommitWrite();

  EXPECT_EQ(sequence_number,
            clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE));
}

}  // namespace content
