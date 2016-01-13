// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_handle_dispatcher.h"

#include <stdio.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "mojo/common/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

TEST(PlatformHandleDispatcherTest, Basic) {
  static const char kHelloWorld[] = "hello world";

  base::FilePath unused;
  base::ScopedFILE fp(CreateAndOpenTemporaryFile(&unused));
  ASSERT_TRUE(fp);
  EXPECT_EQ(sizeof(kHelloWorld),
            fwrite(kHelloWorld, 1, sizeof(kHelloWorld), fp.get()));

  embedder::ScopedPlatformHandle
      h(mojo::test::PlatformHandleFromFILE(fp.Pass()));
  EXPECT_FALSE(fp);
  ASSERT_TRUE(h.is_valid());

  scoped_refptr<PlatformHandleDispatcher> dispatcher(
      new PlatformHandleDispatcher(h.Pass()));
  EXPECT_FALSE(h.is_valid());
  EXPECT_EQ(Dispatcher::kTypePlatformHandle, dispatcher->GetType());

  h = dispatcher->PassPlatformHandle().Pass();
  EXPECT_TRUE(h.is_valid());

  fp = mojo::test::FILEFromPlatformHandle(h.Pass(), "rb").Pass();
  EXPECT_FALSE(h.is_valid());
  EXPECT_TRUE(fp);

  rewind(fp.get());
  char read_buffer[1000] = {};
  EXPECT_EQ(sizeof(kHelloWorld),
            fread(read_buffer, 1, sizeof(read_buffer), fp.get()));
  EXPECT_STREQ(kHelloWorld, read_buffer);

  // Try getting the handle again. (It should fail cleanly.)
  h = dispatcher->PassPlatformHandle().Pass();
  EXPECT_FALSE(h.is_valid());

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

TEST(PlatformHandleDispatcherTest, CreateEquivalentDispatcherAndClose) {
  static const char kFooBar[] = "foo bar";

  base::FilePath unused;
  base::ScopedFILE fp(CreateAndOpenTemporaryFile(&unused));
  EXPECT_EQ(sizeof(kFooBar), fwrite(kFooBar, 1, sizeof(kFooBar), fp.get()));

  scoped_refptr<PlatformHandleDispatcher> dispatcher(
      new PlatformHandleDispatcher(
          mojo::test::PlatformHandleFromFILE(fp.Pass())));

  DispatcherTransport transport(
      test::DispatcherTryStartTransport(dispatcher.get()));
  EXPECT_TRUE(transport.is_valid());
  EXPECT_EQ(Dispatcher::kTypePlatformHandle, transport.GetType());
  EXPECT_FALSE(transport.IsBusy());

  scoped_refptr<Dispatcher> generic_dispatcher =
      transport.CreateEquivalentDispatcherAndClose();
  ASSERT_TRUE(generic_dispatcher);

  transport.End();
  EXPECT_TRUE(dispatcher->HasOneRef());
  dispatcher = NULL;

  ASSERT_EQ(Dispatcher::kTypePlatformHandle, generic_dispatcher->GetType());
  dispatcher = static_cast<PlatformHandleDispatcher*>(generic_dispatcher.get());

  fp = mojo::test::FILEFromPlatformHandle(dispatcher->PassPlatformHandle(),
                                          "rb").Pass();
  EXPECT_TRUE(fp);

  rewind(fp.get());
  char read_buffer[1000] = {};
  EXPECT_EQ(sizeof(kFooBar),
            fread(read_buffer, 1, sizeof(read_buffer), fp.get()));
  EXPECT_STREQ(kFooBar, read_buffer);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

}  // namespace
}  // namespace system
}  // namespace mojo
