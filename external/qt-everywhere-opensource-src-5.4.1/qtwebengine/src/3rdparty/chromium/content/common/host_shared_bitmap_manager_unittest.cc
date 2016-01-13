// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class HostSharedBitmapManagerTest : public testing::Test {
 protected:
  virtual void SetUp() { manager_.reset(new HostSharedBitmapManager()); }
  scoped_ptr<HostSharedBitmapManager> manager_;
};

TEST_F(HostSharedBitmapManagerTest, TestCreate) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  scoped_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  base::SharedMemoryHandle handle;
  bitmap->ShareToProcess(base::GetCurrentProcessHandle(), &handle);
  manager_->ChildAllocatedSharedBitmap(
      size_in_bytes, handle, base::GetCurrentProcessHandle(), id);

  scoped_ptr<cc::SharedBitmap> large_bitmap;
  large_bitmap = manager_->GetSharedBitmapFromId(gfx::Size(1024, 1024), id);
  EXPECT_TRUE(large_bitmap.get() == NULL);

  scoped_ptr<cc::SharedBitmap> very_large_bitmap;
  very_large_bitmap =
      manager_->GetSharedBitmapFromId(gfx::Size(1, (1 << 30) | 1), id);
  EXPECT_TRUE(very_large_bitmap.get() == NULL);

  scoped_ptr<cc::SharedBitmap> negative_size_bitmap;
  negative_size_bitmap =
      manager_->GetSharedBitmapFromId(gfx::Size(-1, 1024), id);
  EXPECT_TRUE(negative_size_bitmap.get() == NULL);

  cc::SharedBitmapId id2 = cc::SharedBitmap::GenerateId();
  scoped_ptr<cc::SharedBitmap> invalid_bitmap;
  invalid_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id2);
  EXPECT_TRUE(invalid_bitmap.get() == NULL);

  scoped_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), 4), 0);

  scoped_ptr<cc::SharedBitmap> large_bitmap2;
  large_bitmap2 = manager_->GetSharedBitmapFromId(gfx::Size(1024, 1024), id);
  EXPECT_TRUE(large_bitmap2.get() == NULL);

  scoped_ptr<cc::SharedBitmap> shared_bitmap2;
  shared_bitmap2 = manager_->GetSharedBitmapFromId(bitmap_size, id);
  EXPECT_TRUE(shared_bitmap2->pixels() == shared_bitmap->pixels());
  shared_bitmap2.reset();
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);

  manager_->ChildDeletedSharedBitmap(id);

  memset(bitmap->memory(), 0, size_in_bytes);

  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);
  bitmap.reset();
  shared_bitmap.reset();
}

TEST_F(HostSharedBitmapManagerTest, TestCreateForChild) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  base::SharedMemoryHandle handle;
  manager_->AllocateSharedBitmapForChild(
      base::GetCurrentProcessHandle(), size_in_bytes, id, &handle);

  EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle));
  scoped_ptr<base::SharedMemory> bitmap(new base::SharedMemory(handle, false));
  EXPECT_TRUE(bitmap->Map(size_in_bytes));
  memset(bitmap->memory(), 0xff, size_in_bytes);

  scoped_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  EXPECT_TRUE(shared_bitmap);
  EXPECT_TRUE(
      memcmp(bitmap->memory(), shared_bitmap->pixels(), size_in_bytes) == 0);
}

TEST_F(HostSharedBitmapManagerTest, RemoveProcess) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  scoped_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  base::SharedMemoryHandle handle;
  bitmap->ShareToProcess(base::GetCurrentProcessHandle(), &handle);
  manager_->ChildAllocatedSharedBitmap(
      size_in_bytes, handle, base::GetCurrentProcessHandle(), id);

  manager_->ProcessRemoved(base::kNullProcessHandle);

  scoped_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);

  manager_->ProcessRemoved(base::GetCurrentProcessHandle());

  scoped_ptr<cc::SharedBitmap> shared_bitmap2;
  shared_bitmap2 = manager_->GetSharedBitmapFromId(bitmap_size, id);
  EXPECT_TRUE(shared_bitmap2.get() == NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);

  shared_bitmap.reset();

  // Should no-op.
  manager_->ChildDeletedSharedBitmap(id);
}

TEST_F(HostSharedBitmapManagerTest, AddDuplicate) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  scoped_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  base::SharedMemoryHandle handle;
  bitmap->ShareToProcess(base::GetCurrentProcessHandle(), &handle);
  manager_->ChildAllocatedSharedBitmap(
      size_in_bytes, handle, base::GetCurrentProcessHandle(), id);

  scoped_ptr<base::SharedMemory> bitmap2(new base::SharedMemory());
  bitmap2->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap2->memory(), 0x00, size_in_bytes);

  manager_->ChildAllocatedSharedBitmap(
      size_in_bytes, bitmap2->handle(), base::GetCurrentProcessHandle(), id);

  scoped_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);
}

}  // namespace
}  // namespace content
