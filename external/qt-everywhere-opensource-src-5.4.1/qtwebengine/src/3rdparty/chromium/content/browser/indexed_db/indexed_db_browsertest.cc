// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/thread_test_helper.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/browser/quota/quota_manager.h"

using base::ASCIIToUTF16;
using quota::QuotaManager;
using webkit_database::DatabaseUtil;

namespace content {

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side.
class IndexedDBBrowserTest : public ContentBrowserTest {
 public:
  IndexedDBBrowserTest() : disk_usage_(-1) {}

  virtual void SetUp() OVERRIDE {
    GetTestClassFactory()->Reset();
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetIDBClassFactory);
    ContentBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(NULL);
    ContentBrowserTest::TearDown();
  }

  void FailOperation(FailClass failure_class,
                     FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num) {
    GetTestClassFactory()->FailOperation(
        failure_class, failure_method, fail_on_instance_num, fail_on_call_num);
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser->web_contents(),
          "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

  void NavigateAndWaitForTitle(Shell* shell,
                               const char* filename,
                               const char* hash,
                               const char* expected_string) {
    GURL url = GetTestUrl("indexeddb", filename);
    if (hash)
      url = GURL(url.spec() + hash);

    base::string16 expected_title16(ASCIIToUTF16(expected_string));
    TitleWatcher title_watcher(shell->web_contents(), expected_title16);
    NavigateToURL(shell, url);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  IndexedDBContextImpl* GetContext() {
    StoragePartition* partition =
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext());
    return static_cast<IndexedDBContextImpl*>(partition->GetIndexedDBContext());
  }

  void SetQuota(int quotaKilobytes) {
    const int kTemporaryStorageQuotaSize = quotaKilobytes
        * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(kTemporaryStorageQuotaSize,
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext())->GetQuotaManager());
  }

  static void SetTempQuota(int64 bytes, scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&IndexedDBBrowserTest::SetTempQuota, bytes, qm));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    qm->SetTemporaryGlobalOverrideQuota(bytes, quota::QuotaCallback());
    // Don't return until the quota has been set.
    scoped_refptr<base::ThreadTestHelper> helper(new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    ASSERT_TRUE(helper->Run());
  }

  virtual int64 RequestDiskUsage() {
    PostTaskAndReplyWithResult(
        GetContext()->TaskRunner(),
        FROM_HERE,
        base::Bind(&IndexedDBContext::GetOriginDiskUsage,
                   GetContext(),
                   GURL("file:///")),
        base::Bind(&IndexedDBBrowserTest::DidGetDiskUsage, this));
    scoped_refptr<base::ThreadTestHelper> helper(new base::ThreadTestHelper(
        BrowserMainLoop::GetInstance()->indexed_db_thread()->
            message_loop_proxy()));
    EXPECT_TRUE(helper->Run());
    // Wait for DidGetDiskUsage to be called.
    base::MessageLoop::current()->RunUntilIdle();
    return disk_usage_;
  }

 private:
  static MockBrowserTestIndexedDBClassFactory* GetTestClassFactory() {
    static ::base::LazyInstance<MockBrowserTestIndexedDBClassFactory>::Leaky
        s_factory = LAZY_INSTANCE_INITIALIZER;
    return s_factory.Pointer();
  }

  static IndexedDBClassFactory* GetIDBClassFactory() {
    return GetTestClassFactory();
  }

  virtual void DidGetDiskUsage(int64 bytes) {
    EXPECT_GT(bytes, 0);
    disk_usage_ = bytes;
  }

  int64 disk_usage_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTest);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTestIncognito) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"),
             true /* incognito */);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorPrefetch) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_prefetch.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, IndexTest) {
  SimpleTest(GetTestUrl("indexeddb", "index_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyPathTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_path_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_get_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyTypesTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_types_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ObjectStoreTest) {
  SimpleTest(GetTestUrl("indexeddb", "object_store_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CallbackAccounting) {
  SimpleTest(GetTestUrl("indexeddb", "callback_accounting.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DoesntHangTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_run_forever.html"));
  CrashTab(shell()->web_contents());
  SimpleTest(GetTestUrl("indexeddb", "transaction_not_blocked.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug84933Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_84933.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug106883Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_106883.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug109187Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_109187.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithLowQuota() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    const int kInitialQuotaKilobytes = 5000;
    SetQuota(kInitialQuotaKilobytes);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithLowQuota);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("indexeddb", "quota_test.html"));
}

class IndexedDBBrowserTestWithGCExposed : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithGCExposed() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithGCExposed);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DatabaseCallbacksTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_callbacks_first.html"));
}

static void CopyLevelDBToProfile(Shell* shell,
                                 scoped_refptr<IndexedDBContextImpl> context,
                                 const std::string& test_directory) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath test_data_dir =
      GetTestFilePath("indexeddb", test_directory.c_str()).Append(leveldb_dir);
  base::FilePath dest = context->data_path().Append(leveldb_dir);
  // If we don't create the destination directory first, the contents of the
  // leveldb directory are copied directly into profile/IndexedDB instead of
  // profile/IndexedDB/file__0.xxx/
  ASSERT_TRUE(base::CreateDirectory(dest));
  const bool kRecursive = true;
  ASSERT_TRUE(base::CopyDirectory(test_data_dir,
                                  context->data_path(),
                                  kRecursive));
}

class IndexedDBBrowserTestWithPreexistingLevelDB : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithPreexistingLevelDB() {}
  virtual void SetUpOnMainThread() OVERRIDE {
    scoped_refptr<IndexedDBContextImpl> context = GetContext();
    context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(
            &CopyLevelDBToProfile, shell(), context, EnclosingLevelDBDir()));
    scoped_refptr<base::ThreadTestHelper> helper(new base::ThreadTestHelper(
        BrowserMainLoop::GetInstance()->indexed_db_thread()->
            message_loop_proxy()));
    ASSERT_TRUE(helper->Run());
  }

  virtual std::string EnclosingLevelDBDir() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithPreexistingLevelDB);
};

class IndexedDBBrowserTestWithVersion0Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "migration_from_0";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion0Schema, MigrationTest) {
  SimpleTest(GetTestUrl("indexeddb", "migration_test.html"));
}

class IndexedDBBrowserTestWithVersion123456Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "schema_version_123456";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion123456Schema,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithVersion987654SSVData : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "ssv_version_987654";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion987654SSVData,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithCorruptLevelDB : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "corrupt_leveldb";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCorruptLevelDB,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithMissingSSTFile : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "missing_sst";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithMissingSSTFile,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_missing_table.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, LevelDBLogFileTest) {
  // Any page that opens an IndexedDB will work here.
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath log_file(FILE_PATH_LITERAL("LOG"));
  base::FilePath log_file_path =
      GetContext()->data_path().Append(leveldb_dir).Append(log_file);
  int64 size;
  EXPECT_TRUE(base::GetFileSize(log_file_path, &size));
  EXPECT_GT(size, 0);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CanDeleteWhenOverQuotaTest) {
  SimpleTest(GetTestUrl("indexeddb", "fill_up_5k.html"));
  int64 size = RequestDiskUsage();
  const int kQuotaKilobytes = 2;
  EXPECT_GT(size, kQuotaKilobytes * 1024);
  SetQuota(kQuotaKilobytes);
  SimpleTest(GetTestUrl("indexeddb", "delete_over_quota.html"));
}

namespace {

static void CompactIndexedDBBackingStore(
    scoped_refptr<IndexedDBContextImpl> context,
    const GURL& origin_url) {
  IndexedDBFactory* factory = context->GetIDBFactory();

  std::pair<IndexedDBFactory::OriginDBMapIterator,
            IndexedDBFactory::OriginDBMapIterator> range =
      factory->GetOpenDatabasesForOrigin(origin_url);

  if (range.first == range.second)  // If no open db's for this origin
    return;

  // Compact the first db's backing store since all the db's are in the same
  // backing store.
  IndexedDBDatabase* db = range.first->second;
  IndexedDBBackingStore* backing_store = db->backing_store();
  backing_store->Compact();
}

static void CorruptIndexedDBDatabase(
    IndexedDBContextImpl* context,
    const GURL& origin_url,
    base::WaitableEvent* signal_when_finished) {

  CompactIndexedDBBackingStore(context, origin_url);

  int numFiles = 0;
  int numErrors = 0;
  base::FilePath idb_data_path = context->GetFilePath(origin_url);
  const bool recursive = false;
  base::FileEnumerator enumerator(
      idb_data_path, recursive, base::FileEnumerator::FILES);
  for (base::FilePath idb_file = enumerator.Next(); !idb_file.empty();
       idb_file = enumerator.Next()) {
    int64 size(0);
    GetFileSize(idb_file, &size);

    if (idb_file.Extension() == FILE_PATH_LITERAL(".ldb")) {
      numFiles++;
      base::File file(idb_file,
                      base::File::FLAG_WRITE | base::File::FLAG_OPEN_TRUNCATED);
      if (file.IsValid()) {
        // Was opened truncated, expand back to the original
        // file size and fill with zeros (corrupting the file).
        file.SetLength(size);
      } else {
        numErrors++;
      }
    }
  }

  VLOG(0) << "There were " << numFiles << " in " << idb_data_path.value()
          << " with " << numErrors << " errors";
  signal_when_finished->Signal();
}

const std::string s_corrupt_db_test_prefix = "/corrupt/test/";

static scoped_ptr<net::test_server::HttpResponse> CorruptDBRequestHandler(
    IndexedDBContextImpl* context,
    const GURL& origin_url,
    const std::string& path,
    IndexedDBBrowserTest* test,
    const net::test_server::HttpRequest& request) {
  std::string request_path;
  if (path.find(s_corrupt_db_test_prefix) != std::string::npos)
    request_path = request.relative_url.substr(s_corrupt_db_test_prefix.size());
  else
    return scoped_ptr<net::test_server::HttpResponse>();

  // Remove the query string if present.
  std::string request_query;
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos) {
    request_query = request_path.substr(query_pos + 1);
    request_path = request_path.substr(0, query_pos);
  }

  if (request_path == "corruptdb" && !request_query.empty()) {
    VLOG(0) << "Requested to corrupt IndexedDB: " << request_query;
    base::WaitableEvent signal_when_finished(false, false);
    context->TaskRunner()->PostTask(FROM_HERE,
                                    base::Bind(&CorruptIndexedDBDatabase,
                                               base::ConstRef(context),
                                               origin_url,
                                               &signal_when_finished));
    signal_when_finished.Wait();

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    return http_response.PassAs<net::test_server::HttpResponse>();
  } else if (request_path == "fail" && !request_query.empty()) {
    FailClass failure_class = FAIL_CLASS_NOTHING;
    FailMethod failure_method = FAIL_METHOD_NOTHING;
    int instance_num = 1;
    int call_num = 1;
    std::string fail_class;
    std::string fail_method;

    url::Component query(0, request_query.length()), key_pos, value_pos;
    while (url::ExtractQueryKeyValue(
        request_query.c_str(), &query, &key_pos, &value_pos)) {
      std::string escaped_key(request_query.substr(key_pos.begin, key_pos.len));
      std::string escaped_value(
          request_query.substr(value_pos.begin, value_pos.len));

      std::string key = net::UnescapeURLComponent(
          escaped_key,
          net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
              net::UnescapeRule::URL_SPECIAL_CHARS);

      std::string value = net::UnescapeURLComponent(
          escaped_value,
          net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
              net::UnescapeRule::URL_SPECIAL_CHARS);

      if (key == "method")
        fail_method = value;
      else if (key == "class")
        fail_class = value;
      else if (key == "instNum")
        instance_num = atoi(value.c_str());
      else if (key == "callNum")
        call_num = atoi(value.c_str());
      else
        NOTREACHED() << "Unknown param: \"" << key << "\"";
    }

    if (fail_class == "LevelDBTransaction") {
      failure_class = FAIL_CLASS_LEVELDB_TRANSACTION;
      if (fail_method == "Get")
        failure_method = FAIL_METHOD_GET;
      else if (fail_method == "Commit")
        failure_method = FAIL_METHOD_COMMIT;
      else {
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
      }
    }

    DCHECK_GE(instance_num, 1);
    DCHECK_GE(call_num, 1);

    test->FailOperation(failure_class, failure_method, instance_num, call_num);

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    return http_response.PassAs<net::test_server::HttpResponse>();
  }

  // A request for a test resource
  base::FilePath resourcePath =
      content::GetTestFilePath("indexeddb", request_path.c_str());
  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  std::string file_contents;
  if (!base::ReadFileToString(resourcePath, &file_contents))
    return scoped_ptr<net::test_server::HttpResponse>();
  http_response->set_content(file_contents);
  return http_response.PassAs<net::test_server::HttpResponse>();
}

}  // namespace

class IndexedDBBrowserCorruptionTest
    : public IndexedDBBrowserTest,
      public ::testing::WithParamInterface<const char*> {};

IN_PROC_BROWSER_TEST_P(IndexedDBBrowserCorruptionTest,
                       OperationOnCorruptedOpenDatabase) {
  ASSERT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndWaitUntilReady());
  const GURL& origin_url = embedded_test_server()->base_url();
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&CorruptDBRequestHandler,
                 base::ConstRef(GetContext()),
                 origin_url,
                 s_corrupt_db_test_prefix,
                 this));

  std::string test_file = s_corrupt_db_test_prefix +
                          "corrupted_open_db_detection.html#" + GetParam();
  SimpleTest(embedded_test_server()->GetURL(test_file));

  test_file = s_corrupt_db_test_prefix + "corrupted_open_db_recovery.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
}

INSTANTIATE_TEST_CASE_P(IndexedDBBrowserCorruptionTestInstantiation,
                        IndexedDBBrowserCorruptionTest,
                        ::testing::Values("get",
                                          "iterate",
                                          "clearObjectStore"));

// Crashes flakily on various platforms. crbug.com/375856
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       DISABLED_DeleteCompactsBackingStore) {
  const GURL test_url = GetTestUrl("indexeddb", "delete_compact.html");
  SimpleTest(GURL(test_url.spec() + "#fill"));
  int64 after_filling = RequestDiskUsage();
  EXPECT_GT(after_filling, 0);

  SimpleTest(GURL(test_url.spec() + "#purge"));
  int64 after_deleting = RequestDiskUsage();
  EXPECT_LT(after_deleting, after_filling);

  // The above tests verify basic assertions - that filling writes data and
  // deleting reduces the amount stored.

  // The below tests make assumptions about implementation specifics, such as
  // data compression, compaction efficiency, and the maximum amount of
  // metadata and log data remains after a deletion. It is possible that
  // changes to the implementation may require these constants to be tweaked.

  const int kTestFillBytes = 1024 * 1024 * 5;  // 5MB
  EXPECT_GT(after_filling, kTestFillBytes);

  const int kTestCompactBytes = 1024 * 1024 * 1;  // 1MB
  EXPECT_LT(after_deleting, kTestCompactBytes);
}

// Complex multi-step (converted from pyauto) tests begin here.

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part1",
                          "pass - first run");
}

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part2",
                          "pass - second run");
}

// Verify that a VERSION_CHANGE transaction is rolled back after a
// renderer/browser crash
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       PRE_PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part1",
                          "pass - part1 - complete");
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part2",
                          "pass - part2 - crash me");
  NavigateToURL(shell(), GURL(kChromeUIBrowserCrashHost));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part3",
                          "pass - part3 - rolled back");
}

// Verify that open DB connections are closed when a tab is destroyed.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ConnectionsClosedOnTabClose) {
  NavigateAndWaitForTitle(shell(), "version_change_blocked.html", "#tab1",
                          "setVersion(2) complete");

  // Start on a different URL to force a new renderer process.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell, GURL(url::kAboutBlankURL));
  NavigateAndWaitForTitle(new_shell, "version_change_blocked.html", "#tab2",
                          "setVersion(3) blocked");

  base::string16 expected_title16(ASCIIToUTF16("setVersion(3) complete"));
  TitleWatcher title_watcher(new_shell->web_contents(), expected_title16);

  base::KillProcess(
      shell()->web_contents()->GetRenderProcessHost()->GetHandle(), 0, true);
  shell()->Close();

  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// Verify that a "close" event is fired at database connections when
// the backing store is deleted.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ForceCloseEventTest) {
  NavigateAndWaitForTitle(shell(), "force_close_event.html", NULL,
                          "connection ready");

  GetContext()->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBContextImpl::DeleteForOrigin,
                 GetContext(),
                 GURL("file:///")));

  base::string16 expected_title16(ASCIIToUTF16("connection closed"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("connection closed with error"));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

class IndexedDBBrowserTestSingleProcess : public IndexedDBBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// Crashing on Android due to kSingleProcess flag: http://crbug.com/342525
#if defined(OS_ANDROID)
#define MAYBE_RenderThreadShutdownTest DISABLED_RenderThreadShutdownTest
#else
#define MAYBE_RenderThreadShutdownTest RenderThreadShutdownTest
#endif
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestSingleProcess,
                       MAYBE_RenderThreadShutdownTest) {
  SimpleTest(GetTestUrl("indexeddb", "shutdown_with_requests.html"));
}

}  // namespace content
