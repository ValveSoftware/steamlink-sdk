// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_usage_store.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Each bucket holds data usage for a 15 minute interval. History is maintained
// for 60 days.
const unsigned kNumExpectedBuckets = 60 * 24 * 60 / 15;
const int kBucketsInHour = 60 / 15;
const int kTestCurrentBucketIndex = 2880;

base::Time::Exploded TestExplodedTime() {
  base::Time::Exploded exploded;
  exploded.year = 2001;
  exploded.month = 12;
  exploded.day_of_month = 31;
  exploded.hour = 12;
  exploded.minute = 1;
  exploded.second = 0;
  exploded.millisecond = 0;

  return exploded;
}
}

namespace data_reduction_proxy {

class DataUsageStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    store_.reset(new TestDataStore());
    data_usage_store_.reset(new DataUsageStore(store_.get()));
  }

  int current_bucket_index() const {
    return data_usage_store_->current_bucket_index_;
  }

  int64_t current_bucket_last_updated() const {
    return data_usage_store_->current_bucket_last_updated_.ToInternalValue();
  }

  int ComputeBucketIndex(const base::Time& time) const {
    return data_usage_store_->ComputeBucketIndex(time);
  }

  TestDataStore* store() const { return store_.get(); }

  DataUsageStore* data_usage_store() const { return data_usage_store_.get(); }

  void PopulateStore() {
    base::Time::Exploded exploded = TestExplodedTime();
    base::Time current_time = base::Time::FromUTCExploded(exploded);

    DataUsageBucket current_bucket;
    current_bucket.set_last_updated_timestamp(current_time.ToInternalValue());
    std::string bucket_value;
    ASSERT_TRUE(current_bucket.SerializeToString(&bucket_value));

    std::map<std::string, std::string> map;
    map.insert(
        std::pair<std::string, std::string>("current_bucket_index", "2880"));
    for (int i = 0; i < static_cast<int>(kNumExpectedBuckets); ++i) {
      base::Time time = current_time - base::TimeDelta::FromMinutes(i * 5);
      DataUsageBucket bucket;
      bucket.set_last_updated_timestamp(time.ToInternalValue());
      int index = kTestCurrentBucketIndex - i;
      if (index < 0)
        index += kNumExpectedBuckets;

      std::string bucket_value;
      ASSERT_TRUE(bucket.SerializeToString(&bucket_value));
      map.insert(std::pair<std::string, std::string>(
          base::StringPrintf("data_usage_bucket:%d", index), bucket_value));
    }
    store()->Put(map);
  }

 private:
  std::unique_ptr<TestDataStore> store_;
  std::unique_ptr<DataUsageStore> data_usage_store_;
};

TEST_F(DataUsageStoreTest, LoadEmpty) {
  ASSERT_NE(0, current_bucket_index());

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  ASSERT_EQ(0, current_bucket_index());
  ASSERT_EQ(0, current_bucket_last_updated());
  ASSERT_FALSE(bucket.has_last_updated_timestamp());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, LoadAndStoreToSameBucket) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  base::Time now = base::Time::Now();
  bucket.set_last_updated_timestamp(now.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  DataUsageBucket stored_bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&stored_bucket);
  ASSERT_EQ(now.ToInternalValue(), stored_bucket.last_updated_timestamp());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 2].has_last_updated_timestamp());
  ASSERT_EQ(now.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreSameBucket) {
  base::Time::Exploded exploded = TestExplodedTime();

  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time time1 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 14;
  exploded.second = 59;
  exploded.millisecond = 999;
  base::Time time2 = base::Time::FromUTCExploded(exploded);

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(time1.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  bucket.set_last_updated_timestamp(time2.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 2].has_last_updated_timestamp());
  ASSERT_EQ(time2.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreConsecutiveBuckets) {
  base::Time::Exploded exploded = TestExplodedTime();

  exploded.minute = 0;
  exploded.second = 59;
  exploded.millisecond = 999;
  base::Time time1 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 15;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time time2 = base::Time::FromUTCExploded(exploded);

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(time1.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  bucket.set_last_updated_timestamp(time2.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(3u, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 3].has_last_updated_timestamp());
  ASSERT_EQ(time1.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 2].last_updated_timestamp());
  ASSERT_EQ(time2.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreMultipleBuckets) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  // Comments indicate time expressed as day.hour.min.sec.millis relative to
  // each other beginning at 0.0.0.0.0.
  // The first bucket range is 0.0.0.0.0 - 0.0.14.59.999 and
  // the second bucket range is 0.0.5.0.0 - 0.0.29.59.999, etc.
  base::Time first_bucket_time = base::Time::Now();  // 0.0.0.0.0.
  base::Time last_bucket_time = first_bucket_time    // 59.23.55.0.0
                                + base::TimeDelta::FromDays(60) -
                                base::TimeDelta::FromMinutes(15);
  base::Time before_history_time =  // 0.0.-5.0.0
      first_bucket_time - base::TimeDelta::FromMinutes(15);
  base::Time tenth_bucket_time =  // 0.2.15.0.0
      first_bucket_time + base::TimeDelta::FromHours(2) +
      base::TimeDelta::FromMinutes(15);
  base::Time second_last_bucket_time =  // 59.23.45.0.0
      last_bucket_time - base::TimeDelta::FromMinutes(15);

  // This bucket will be discarded when the |last_bucket| is stored.
  DataUsageBucket bucket_before_history;
  bucket_before_history.set_last_updated_timestamp(
      before_history_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket_before_history);
  // Only one bucket has been stored, so store should have 2 entries, one for
  // current index and one for the bucket itself.
  ASSERT_EQ(2u, store()->map()->size());

  // Calling |StoreCurrentDataUsageBucket| with the same last updated timestamp
  // should not cause number of stored buckets to increase and current bucket
  // should be overwritten.
  data_usage_store()->StoreCurrentDataUsageBucket(bucket_before_history);
  // Only one bucket has been stored, so store should have 2 entries, one for
  // current index and one for the bucket itself.
  ASSERT_EQ(2u, store()->map()->size());

  // This will be the very first bucket once |last_bucket| is stored.
  DataUsageBucket first_bucket;
  first_bucket.set_last_updated_timestamp(first_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(first_bucket);
  // A new bucket has been stored, so entires in map should increase by one.
  ASSERT_EQ(3u, store()->map()->size());

  // This will be the 10th bucket when |last_bucket| is stored. We skipped
  // calling |StoreCurrentDataUsageBucket| on 10 buckets.
  DataUsageBucket tenth_bucket;
  tenth_bucket.set_last_updated_timestamp(tenth_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(tenth_bucket);
  // 9 more (empty) buckets should have been added to the store.
  ASSERT_EQ(12u, store()->map()->size());

  // This will be the last but one bucket when |last_bucket| is stored.
  DataUsageBucket second_last_bucket;
  second_last_bucket.set_last_updated_timestamp(
      second_last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(second_last_bucket);
  // Max number of buckets we store to DB plus one for the current index.
  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  // This bucket should simply overwrite oldest bucket, so number of entries in
  // store should be unchanged.
  DataUsageBucket last_bucket;
  last_bucket.set_last_updated_timestamp(last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(last_bucket);
  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_EQ(first_bucket_time.ToInternalValue(),
            data_usage[0].last_updated_timestamp());
  ASSERT_EQ(tenth_bucket_time.ToInternalValue(),
            data_usage[9].last_updated_timestamp());
  ASSERT_EQ(second_last_bucket_time.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 2].last_updated_timestamp());
  ASSERT_EQ(last_bucket_time.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, DeleteHistoricalDataUsage) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(base::Time::Now().ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  data_usage_store()->DeleteHistoricalDataUsage();
  ASSERT_EQ(0u, store()->map()->size());
}

TEST_F(DataUsageStoreTest, BucketOverlapsInterval) {
  base::Time::Exploded exploded = TestExplodedTime();
  base::Time time1 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 16;
  base::Time time16 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 17;
  base::Time time17 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 18;
  base::Time time18 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 33;
  base::Time time33 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 34;
  base::Time time34 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 46;
  base::Time time46 = base::Time::FromUTCExploded(exploded);

  ASSERT_FALSE(DataUsageStore::BucketOverlapsInterval(time1, time17, time33));
  ASSERT_TRUE(DataUsageStore::BucketOverlapsInterval(time16, time17, time33));
  ASSERT_TRUE(DataUsageStore::BucketOverlapsInterval(time18, time17, time33));
  ASSERT_TRUE(DataUsageStore::BucketOverlapsInterval(time34, time17, time33));
  ASSERT_FALSE(DataUsageStore::BucketOverlapsInterval(time46, time17, time33));
}

TEST_F(DataUsageStoreTest, ComputeBucketIndex) {
  PopulateStore();

  base::Time::Exploded exploded = TestExplodedTime();

  DataUsageBucket current_bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&current_bucket);

  exploded.minute = 0;
  ASSERT_EQ(kTestCurrentBucketIndex,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));
  exploded.minute = 14;
  ASSERT_EQ(kTestCurrentBucketIndex,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.minute = 15;
  ASSERT_EQ(kTestCurrentBucketIndex + 1,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.hour = 11;
  exploded.minute = 59;
  ASSERT_EQ(kTestCurrentBucketIndex - 1,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.minute = 0;
  ASSERT_EQ(kTestCurrentBucketIndex - kBucketsInHour,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.hour = 1;
  ASSERT_EQ(kTestCurrentBucketIndex - kBucketsInHour * 11,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.day_of_month = 1;
  exploded.hour = 12;
  ASSERT_EQ(kTestCurrentBucketIndex - kBucketsInHour * 30 * 24,
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.hour = 11;
  exploded.minute = 45;
  ASSERT_EQ(kTestCurrentBucketIndex - kBucketsInHour * 30 * 24 - 1 +
                static_cast<int>(kNumExpectedBuckets),
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));

  exploded.minute = 30;
  ASSERT_EQ(kTestCurrentBucketIndex - kBucketsInHour * 30 * 24 - 2 +
                static_cast<int>(kNumExpectedBuckets),
            ComputeBucketIndex(base::Time::FromUTCExploded(exploded)));
}

TEST_F(DataUsageStoreTest, DeleteBrowsingHistory) {
  PopulateStore();
  DataUsageBucket current_bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&current_bucket);

  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  base::Time::Exploded exploded = TestExplodedTime();
  exploded.minute = 0;
  base::Time now = base::Time::FromUTCExploded(exploded);
  base::Time fifteen_mins_from_now = now + base::TimeDelta::FromMinutes(15);
  // Deleting browsing from the future should be a no-op.
  data_usage_store()->DeleteBrowsingHistory(fifteen_mins_from_now,
                                            fifteen_mins_from_now);
  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kTestCurrentBucketIndex)) !=
              store()->map()->end());
  // Delete the current bucket.
  data_usage_store()->DeleteBrowsingHistory(now, now);
  ASSERT_EQ(kNumExpectedBuckets, store()->map()->size());
  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kTestCurrentBucketIndex)) ==
              store()->map()->end());

  data_usage_store()->DeleteBrowsingHistory(
      now - base::TimeDelta::FromMinutes(5), now);
  ASSERT_EQ(kNumExpectedBuckets - 1, store()->map()->size());
  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kTestCurrentBucketIndex - 1)) ==
              store()->map()->end());

  data_usage_store()->DeleteBrowsingHistory(now - base::TimeDelta::FromDays(30),
                                            now);
  ASSERT_EQ(kNumExpectedBuckets - kBucketsInHour * 30 * 24,
            store()->map()->size());
  ASSERT_TRUE(store()->map()->find("data_usage_bucket:0") ==
              store()->map()->end());
  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kNumExpectedBuckets - 1)) !=
              store()->map()->end());

  // Delete wraps around and removes the last element which is at position
  // (|kNumExpectedBuckets| - 1).
  data_usage_store()->DeleteBrowsingHistory(
      now - base::TimeDelta::FromDays(30) - base::TimeDelta::FromMinutes(5),
      now);
  ASSERT_EQ(kNumExpectedBuckets - kBucketsInHour * 30 * 24 - 1,
            store()->map()->size());
  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kNumExpectedBuckets - 1)) ==
              store()->map()->end());
  ASSERT_TRUE(store()->map()->find(base::StringPrintf(
                  "data_usage_bucket:%d", kNumExpectedBuckets - 2)) !=
              store()->map()->end());

  data_usage_store()->DeleteBrowsingHistory(now - base::TimeDelta::FromDays(60),
                                            now);
  ASSERT_EQ(1u, store()->map()->size());
}

}  // namespace data_reduction_proxy
