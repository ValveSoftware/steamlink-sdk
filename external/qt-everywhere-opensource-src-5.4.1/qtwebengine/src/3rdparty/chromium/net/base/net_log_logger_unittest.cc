// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class NetLogLoggerTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.path().AppendASCII("NetLogFile");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(NetLogLoggerTest, GeneratesValidJSONForNoEvents) {
  {
    // Create and destroy a logger.
    FILE* file = base::OpenFile(log_path_, "w");
    ASSERT_TRUE(file);
    scoped_ptr<base::Value> constants(NetLogLogger::GetConstants());
    NetLogLogger logger(file, *constants);
  }

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(0u, events->GetSize());
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithOneEvent) {
  {
    FILE* file = base::OpenFile(log_path_, "w");
    ASSERT_TRUE(file);
    scoped_ptr<base::Value> constants(NetLogLogger::GetConstants());
    NetLogLogger logger(file, *constants);

    const int kDummyId = 1;
    NetLog::Source source(NetLog::SOURCE_SPDY_SESSION, kDummyId);
    NetLog::EntryData entry_data(NetLog::TYPE_PROXY_SERVICE,
                                 source,
                                 NetLog::PHASE_BEGIN,
                                 base::TimeTicks::Now(),
                                 NULL);
    NetLog::Entry entry(&entry_data, NetLog::LOG_ALL);
    logger.OnAddEntry(entry);
  }

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(1u, events->GetSize());
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithMultipleEvents) {
  {
    FILE* file = base::OpenFile(log_path_, "w");
    ASSERT_TRUE(file);
    scoped_ptr<base::Value> constants(NetLogLogger::GetConstants());
    NetLogLogger logger(file, *constants);

    const int kDummyId = 1;
    NetLog::Source source(NetLog::SOURCE_SPDY_SESSION, kDummyId);
    NetLog::EntryData entry_data(NetLog::TYPE_PROXY_SERVICE,
                                 source,
                                 NetLog::PHASE_BEGIN,
                                 base::TimeTicks::Now(),
                                 NULL);
    NetLog::Entry entry(&entry_data, NetLog::LOG_ALL);

    // Add the entry multiple times.
    logger.OnAddEntry(entry);
    logger.OnAddEntry(entry);
  }

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(2u, events->GetSize());
}

}  // namespace net
