// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

}  // namespace

class ShaderDiskCacheTest : public testing::Test {
 public:
  ShaderDiskCacheTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }

  ~ShaderDiskCacheTest() override {}

  const base::FilePath& cache_path() { return temp_dir_.path(); }

  void InitCache() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ShaderCacheFactory::GetInstance()->SetCacheInfo(kDefaultClientId,
                                                        cache_path());
  }

 private:
  void TearDown() override {
    ShaderCacheFactory::GetInstance()->RemoveCacheInfo(kDefaultClientId);
  }

  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCacheTest);
};

TEST_F(ShaderDiskCacheTest, ClearsCache) {
  InitCache();

  scoped_refptr<ShaderDiskCache> cache =
      ShaderCacheFactory::GetInstance()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != NULL);

  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());

  cache->Cache(kCacheKey, kCacheValue);

  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(1, cache->Size());

  base::Time time;
  net::TestCompletionCallback clear_cb;
  rv = cache->Clear(time, time, clear_cb.callback());
  ASSERT_EQ(net::OK, clear_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());
};

}  // namespace content
