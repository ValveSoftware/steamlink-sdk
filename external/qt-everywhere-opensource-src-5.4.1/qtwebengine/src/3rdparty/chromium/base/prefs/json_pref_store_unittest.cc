// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/json_pref_store.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_filter.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

const char kHomePage[] = "homepage";

// A PrefFilter that will intercept all calls to FilterOnLoad() and hold on
// to the |prefs| until explicitly asked to release them.
class InterceptingPrefFilter : public PrefFilter {
 public:
  InterceptingPrefFilter();
  virtual ~InterceptingPrefFilter();

  // PrefFilter implementation:
  virtual void FilterOnLoad(
      const PostFilterOnLoadCallback& post_filter_on_load_callback,
      scoped_ptr<base::DictionaryValue> pref_store_contents) OVERRIDE;
  virtual void FilterUpdate(const std::string& path) OVERRIDE {}
  virtual void FilterSerializeData(
      base::DictionaryValue* pref_store_contents) OVERRIDE {}

  bool has_intercepted_prefs() const { return intercepted_prefs_ != NULL; }

  // Finalize an intercepted read, handing |intercepted_prefs_| back to its
  // JsonPrefStore.
  void ReleasePrefs();

 private:
  PostFilterOnLoadCallback post_filter_on_load_callback_;
  scoped_ptr<base::DictionaryValue> intercepted_prefs_;

  DISALLOW_COPY_AND_ASSIGN(InterceptingPrefFilter);
};

InterceptingPrefFilter::InterceptingPrefFilter() {}
InterceptingPrefFilter::~InterceptingPrefFilter() {}

void InterceptingPrefFilter::FilterOnLoad(
    const PostFilterOnLoadCallback& post_filter_on_load_callback,
    scoped_ptr<base::DictionaryValue> pref_store_contents) {
  post_filter_on_load_callback_ = post_filter_on_load_callback;
  intercepted_prefs_ = pref_store_contents.Pass();
}

void InterceptingPrefFilter::ReleasePrefs() {
  EXPECT_FALSE(post_filter_on_load_callback_.is_null());
  post_filter_on_load_callback_.Run(intercepted_prefs_.Pass(), false);
  post_filter_on_load_callback_.Reset();
}

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void (const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void (bool));
};

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  MOCK_METHOD1(OnError, void(PersistentPrefStore::PrefReadError));
};

}  // namespace

class JsonPrefStoreTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("prefs");
    ASSERT_TRUE(PathExists(data_dir_));
  }

  virtual void TearDown() OVERRIDE {
    // Make sure all pending tasks have been processed (e.g., deleting the
    // JsonPrefStore may post write tasks).
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitWhenIdleClosure());
    message_loop_.Run();
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored.
  base::FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
};

// Test fallback behavior for a nonexistent file.
TEST_F(JsonPrefStoreTest, NonExistentFile) {
  base::FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      bogus_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
}

// Test fallback behavior for a nonexistent file and alternate file.
TEST_F(JsonPrefStoreTest, NonExistentFileAndAlternateFile) {
  base::FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  base::FilePath bogus_alternate_input_file =
      data_dir_.AppendASCII("read_alternate.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  ASSERT_FALSE(PathExists(bogus_alternate_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      bogus_input_file,
      bogus_alternate_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
}

// Test fallback behavior for an invalid file.
TEST_F(JsonPrefStoreTest, InvalidFile) {
  base::FilePath invalid_file_original = data_dir_.AppendASCII("invalid.json");
  base::FilePath invalid_file = temp_dir_.path().AppendASCII("invalid.json");
  ASSERT_TRUE(base::CopyFile(invalid_file_original, invalid_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(invalid_file,
                        message_loop_.message_loop_proxy().get(),
                        scoped_ptr<PrefFilter>());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The file should have been moved aside.
  EXPECT_FALSE(PathExists(invalid_file));
  base::FilePath moved_aside = temp_dir_.path().AppendASCII("invalid.bad");
  EXPECT_TRUE(PathExists(moved_aside));
  EXPECT_TRUE(TextContentsEqual(invalid_file_original, moved_aside));
}

// This function is used to avoid code duplication while testing synchronous and
// asynchronous version of the JsonPrefStore loading.
void RunBasicJsonPrefStoreTest(JsonPrefStore* pref_store,
                               const base::FilePath& output_file,
                               const base::FilePath& golden_output_file) {
  const char kNewWindowsInTabs[] = "tabs.new_windows_in_tabs";
  const char kMaxTabs[] = "tabs.max_tabs";
  const char kLongIntPref[] = "long_int.pref";

  std::string cnn("http://www.cnn.com");

  const Value* actual;
  EXPECT_TRUE(pref_store->GetValue(kHomePage, &actual));
  std::string string_value;
  EXPECT_TRUE(actual->GetAsString(&string_value));
  EXPECT_EQ(cnn, string_value);

  const char kSomeDirectory[] = "some_directory";

  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  base::FilePath::StringType path;
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")), path);
  base::FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));

  pref_store->SetValue(kSomeDirectory, new StringValue(some_path.value()));
  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(some_path.value(), path);

  // Test reading some other data types from sub-dictionaries.
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  bool boolean = false;
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_TRUE(boolean);

  pref_store->SetValue(kNewWindowsInTabs, new FundamentalValue(false));
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_FALSE(boolean);

  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  int integer = 0;
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(20, integer);
  pref_store->SetValue(kMaxTabs, new FundamentalValue(10));
  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(10, integer);

  pref_store->SetValue(kLongIntPref,
                       new StringValue(base::Int64ToString(214748364842LL)));
  EXPECT_TRUE(pref_store->GetValue(kLongIntPref, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_value));
  int64 value;
  base::StringToInt64(string_value, &value);
  EXPECT_EQ(214748364842LL, value);

  // Serialize and compare to expected output.
  ASSERT_TRUE(PathExists(golden_output_file));
  pref_store->CommitPendingWrite();
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(TextContentsEqual(golden_output_file, output_file));
  ASSERT_TRUE(base::DeleteFile(output_file, false));
}

TEST_F(JsonPrefStoreTest, Basic) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                             temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
  EXPECT_TRUE(pref_store->IsInitializationComplete());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, BasicAsync) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                             temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());

  {
    MockPrefStoreObserver mock_observer;
    pref_store->AddObserver(&mock_observer);

    MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    EXPECT_CALL(*mock_error_delegate,
                OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    RunLoop().RunUntilIdle();
    pref_store->RemoveObserver(&mock_observer);

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_TRUE(pref_store->IsInitializationComplete());
  }

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, PreserveEmptyValues) {
  FilePath pref_file = temp_dir_.path().AppendASCII("empty_values.json");

  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      pref_file,
      message_loop_.message_loop_proxy(),
      scoped_ptr<PrefFilter>());

  // Set some keys with empty values.
  pref_store->SetValue("list", new base::ListValue);
  pref_store->SetValue("dict", new base::DictionaryValue);

  // Write to file.
  pref_store->CommitPendingWrite();
  MessageLoop::current()->RunUntilIdle();

  // Reload.
  pref_store = new JsonPrefStore(
      pref_file,
      message_loop_.message_loop_proxy(),
      scoped_ptr<PrefFilter>());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  ASSERT_FALSE(pref_store->ReadOnly());

  // Check values.
  const Value* result = NULL;
  EXPECT_TRUE(pref_store->GetValue("list", &result));
  EXPECT_TRUE(ListValue().Equals(result));
  EXPECT_TRUE(pref_store->GetValue("dict", &result));
  EXPECT_TRUE(DictionaryValue().Equals(result));
}

// This test is just documenting some potentially non-obvious behavior. It
// shouldn't be taken as normative.
TEST_F(JsonPrefStoreTest, RemoveClearsEmptyParent) {
  FilePath pref_file = temp_dir_.path().AppendASCII("empty_values.json");

  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      pref_file,
      message_loop_.message_loop_proxy(),
      scoped_ptr<PrefFilter>());

  base::DictionaryValue* dict = new base::DictionaryValue;
  dict->SetString("key", "value");
  pref_store->SetValue("dict", dict);

  pref_store->RemoveValue("dict.key");

  const base::Value* retrieved_dict = NULL;
  bool has_dict = pref_store->GetValue("dict", &retrieved_dict);
  EXPECT_FALSE(has_dict);
}

// Tests asynchronous reading of the file when there is no file.
TEST_F(JsonPrefStoreTest, AsyncNonExistingFile) {
  base::FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      bogus_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());
  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);

  MockReadErrorDelegate *mock_error_delegate = new MockReadErrorDelegate;
  pref_store->ReadPrefsAsync(mock_error_delegate);

  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  EXPECT_CALL(*mock_error_delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_NO_FILE)).Times(1);
  RunLoop().RunUntilIdle();
  pref_store->RemoveObserver(&mock_observer);

  EXPECT_FALSE(pref_store->ReadOnly());
}

TEST_F(JsonPrefStoreTest, ReadWithInterceptor) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                             temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));

  scoped_ptr<InterceptingPrefFilter> intercepting_pref_filter(
      new InterceptingPrefFilter());
  InterceptingPrefFilter* raw_intercepting_pref_filter_ =
      intercepting_pref_filter.get();
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(input_file,
                        message_loop_.message_loop_proxy().get(),
                        intercepting_pref_filter.PassAs<PrefFilter>());

  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The store shouldn't be considered initialized until the interceptor
  // returns.
  EXPECT_TRUE(raw_intercepting_pref_filter_->has_intercepted_prefs());
  EXPECT_FALSE(pref_store->IsInitializationComplete());
  EXPECT_FALSE(pref_store->GetValue(kHomePage, NULL));

  raw_intercepting_pref_filter_->ReleasePrefs();

  EXPECT_FALSE(raw_intercepting_pref_filter_->has_intercepted_prefs());
  EXPECT_TRUE(pref_store->IsInitializationComplete());
  EXPECT_TRUE(pref_store->GetValue(kHomePage, NULL));

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, ReadAsyncWithInterceptor) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                             temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));

  scoped_ptr<InterceptingPrefFilter> intercepting_pref_filter(
      new InterceptingPrefFilter());
  InterceptingPrefFilter* raw_intercepting_pref_filter_ =
      intercepting_pref_filter.get();
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(input_file,
                        message_loop_.message_loop_proxy().get(),
                        intercepting_pref_filter.PassAs<PrefFilter>());

  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);

  // Ownership of the |mock_error_delegate| is handed to the |pref_store| below.
  MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;

  {
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(0);
    // EXPECT_CALL(*mock_error_delegate,
    //             OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    RunLoop().RunUntilIdle();

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_TRUE(raw_intercepting_pref_filter_->has_intercepted_prefs());
    EXPECT_FALSE(pref_store->IsInitializationComplete());
    EXPECT_FALSE(pref_store->GetValue(kHomePage, NULL));
  }

  {
    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    // EXPECT_CALL(*mock_error_delegate,
    //             OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);

    raw_intercepting_pref_filter_->ReleasePrefs();

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_FALSE(raw_intercepting_pref_filter_->has_intercepted_prefs());
    EXPECT_TRUE(pref_store->IsInitializationComplete());
    EXPECT_TRUE(pref_store->GetValue(kHomePage, NULL));
  }

  pref_store->RemoveObserver(&mock_observer);

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, AlternateFile) {
  ASSERT_TRUE(
      base::CopyFile(data_dir_.AppendASCII("read.json"),
                     temp_dir_.path().AppendASCII("alternate.json")));

  // Test that the alternate file is moved to the main file and read as-is from
  // there.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  base::FilePath alternate_input_file =
      temp_dir_.path().AppendASCII("alternate.json");
  ASSERT_FALSE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      alternate_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());

  ASSERT_FALSE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_FALSE(PathExists(alternate_input_file));

  EXPECT_FALSE(pref_store->ReadOnly());
  EXPECT_TRUE(pref_store->IsInitializationComplete());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, AlternateFileIgnoredWhenMainFileExists) {
  ASSERT_TRUE(
      base::CopyFile(data_dir_.AppendASCII("read.json"),
                     temp_dir_.path().AppendASCII("write.json")));
  ASSERT_TRUE(
      base::CopyFile(data_dir_.AppendASCII("invalid.json"),
                     temp_dir_.path().AppendASCII("alternate.json")));

  // Test that the alternate file is ignored and that the read occurs from the
  // existing main file. There is no attempt at even deleting the alternate
  // file as this scenario should never happen in normal user-data-dirs.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  base::FilePath alternate_input_file =
      temp_dir_.path().AppendASCII("alternate.json");
  ASSERT_TRUE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      alternate_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));

  EXPECT_FALSE(pref_store->ReadOnly());
  EXPECT_TRUE(pref_store->IsInitializationComplete());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, AlternateFileDNE) {
  ASSERT_TRUE(
      base::CopyFile(data_dir_.AppendASCII("read.json"),
                     temp_dir_.path().AppendASCII("write.json")));

  // Test that the basic read works fine when an alternate file is specified but
  // does not exist.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  base::FilePath alternate_input_file =
      temp_dir_.path().AppendASCII("alternate.json");
  ASSERT_TRUE(PathExists(input_file));
  ASSERT_FALSE(PathExists(alternate_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      alternate_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_FALSE(PathExists(alternate_input_file));
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_FALSE(PathExists(alternate_input_file));

  EXPECT_FALSE(pref_store->ReadOnly());
  EXPECT_TRUE(pref_store->IsInitializationComplete());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, BasicAsyncWithAlternateFile) {
  ASSERT_TRUE(
      base::CopyFile(data_dir_.AppendASCII("read.json"),
                     temp_dir_.path().AppendASCII("alternate.json")));

  // Test that the alternate file is moved to the main file and read as-is from
  // there even when the read is made asynchronously.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  base::FilePath alternate_input_file =
      temp_dir_.path().AppendASCII("alternate.json");
  ASSERT_FALSE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      input_file,
      alternate_input_file,
      message_loop_.message_loop_proxy().get(),
      scoped_ptr<PrefFilter>());

  ASSERT_FALSE(PathExists(input_file));
  ASSERT_TRUE(PathExists(alternate_input_file));

  {
    MockPrefStoreObserver mock_observer;
    pref_store->AddObserver(&mock_observer);

    MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    EXPECT_CALL(*mock_error_delegate,
                OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    RunLoop().RunUntilIdle();
    pref_store->RemoveObserver(&mock_observer);

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_TRUE(pref_store->IsInitializationComplete());
  }

  ASSERT_TRUE(PathExists(input_file));
  ASSERT_FALSE(PathExists(alternate_input_file));

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

}  // namespace base
