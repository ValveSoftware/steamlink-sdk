// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "components/mus/common/gpu_type_converters.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

// Test for mojo TypeConverter of mus::mojom::ChannelHandle.
TEST(MusGpuTypeConvertersTest, ChannelHandle) {
  {
    const std::string channel_name = "test_channel_name";
    IPC::ChannelHandle handle(channel_name);

    mus::mojom::ChannelHandlePtr mojo_handle =
        mus::mojom::ChannelHandle::From(handle);
    ASSERT_EQ(mojo_handle->name, channel_name);
    EXPECT_FALSE(mojo_handle->socket.is_valid());

    handle = mojo_handle.To<IPC::ChannelHandle>();
    ASSERT_EQ(handle.name, channel_name);
#if defined(OS_POSIX)
    ASSERT_EQ(handle.socket.fd, -1);
#endif
  }

#if defined(OS_POSIX)
  {
    const std::string channel_name = "test_channel_name";
    int fd1 = -1;
    int fd2 = -1;
    bool result = IPC::SocketPair(&fd1, &fd2);
    EXPECT_TRUE(result);

    base::ScopedFD scoped_fd1(fd1);
    base::ScopedFD scoped_fd2(fd2);
    IPC::ChannelHandle handle(channel_name,
                              base::FileDescriptor(scoped_fd1.release(), true));

    mus::mojom::ChannelHandlePtr mojo_handle =
        mus::mojom::ChannelHandle::From(handle);
    ASSERT_EQ(mojo_handle->name, channel_name);
    EXPECT_TRUE(mojo_handle->socket.is_valid());

    handle = mojo_handle.To<IPC::ChannelHandle>();
    ASSERT_EQ(handle.name, channel_name);
    ASSERT_NE(handle.socket.fd, -1);
    EXPECT_TRUE(handle.socket.auto_close);
    base::ScopedFD socped_fd3(handle.socket.fd);
  }
#endif
}

// Test for mojo TypeConverter of mus::mojom::GpuMemoryBufferHandle
TEST(MusGpuTypeConvertersTest, GpuMemoryBufferHandle) {
  const gfx::GpuMemoryBufferId kId(99);
  const uint32_t kOffset = 126;
  const int32_t kStride = 256;
  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAnonymous(1024));
  ASSERT_TRUE(shared_memory.Map(1024));

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = kId;
  handle.handle = base::SharedMemory::DuplicateHandle(shared_memory.handle());
  handle.offset = kOffset;
  handle.stride = kStride;

  mus::mojom::GpuMemoryBufferHandlePtr gpu_handle =
      mus::mojom::GpuMemoryBufferHandle::From<gfx::GpuMemoryBufferHandle>(
          handle);
  ASSERT_EQ(gpu_handle->type, mus::mojom::GpuMemoryBufferType::SHARED_MEMORY);
  ASSERT_EQ(gpu_handle->id->id, 99);
  ASSERT_EQ(gpu_handle->offset, kOffset);
  ASSERT_EQ(gpu_handle->stride, kStride);

  handle = gpu_handle.To<gfx::GpuMemoryBufferHandle>();
  ASSERT_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  ASSERT_EQ(handle.id, kId);
  ASSERT_EQ(handle.offset, kOffset);
  ASSERT_EQ(handle.stride, kStride);

  base::SharedMemory shared_memory1(handle.handle, true);
  ASSERT_TRUE(shared_memory1.Map(1024));
}
