// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/mock_download_file.h"
#include "content/public/browser/download_destination_observer.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

const int kDownloadChunkSize = 1000;
const int kDownloadSpeed = 1000;
const base::FilePath::CharType kDummyPath[] = FILE_PATH_LITERAL("/testpath");

namespace content {

namespace {

class MockDelegate : public DownloadItemImplDelegate {
 public:
  MockDelegate() : DownloadItemImplDelegate() {
    SetDefaultExpectations();
  }

  MOCK_METHOD2(DetermineDownloadTarget, void(
      DownloadItemImpl*, const DownloadTargetCallback&));
  MOCK_METHOD2(ShouldCompleteDownload,
               bool(DownloadItemImpl*, const base::Closure&));
  MOCK_METHOD2(ShouldOpenDownload,
               bool(DownloadItemImpl*, const ShouldOpenDownloadCallback&));
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const base::FilePath&));
  MOCK_METHOD1(CheckForFileRemoval, void(DownloadItemImpl*));

  virtual void ResumeInterruptedDownload(
      scoped_ptr<DownloadUrlParameters> params, uint32 id) OVERRIDE {
    MockResumeInterruptedDownload(params.get(), id);
  }
  MOCK_METHOD2(MockResumeInterruptedDownload,
               void(DownloadUrlParameters* params, uint32 id));

  MOCK_CONST_METHOD0(GetBrowserContext, BrowserContext*());
  MOCK_METHOD1(UpdatePersistence, void(DownloadItemImpl*));
  MOCK_METHOD1(DownloadOpened, void(DownloadItemImpl*));
  MOCK_METHOD1(DownloadRemoved, void(DownloadItemImpl*));
  MOCK_CONST_METHOD1(AssertStateConsistent, void(DownloadItemImpl*));

  void VerifyAndClearExpectations() {
    ::testing::Mock::VerifyAndClearExpectations(this);
    SetDefaultExpectations();
  }

 private:
  void SetDefaultExpectations() {
    EXPECT_CALL(*this, AssertStateConsistent(_))
        .WillRepeatedly(Return());
    EXPECT_CALL(*this, ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldOpenDownload(_, _))
        .WillRepeatedly(Return(true));
  }
};

class MockRequestHandle : public DownloadRequestHandleInterface {
 public:
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(PauseRequest, void());
  MOCK_CONST_METHOD0(ResumeRequest, void());
  MOCK_CONST_METHOD0(CancelRequest, void());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

// Schedules a task to invoke the RenameCompletionCallback with |new_path| on
// the UI thread. Should only be used as the action for
// MockDownloadFile::Rename as follows:
//   EXPECT_CALL(download_file, Rename*(_,_))
//       .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
//                                        new_path));
ACTION_P2(ScheduleRenameCallback, interrupt_reason, new_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(arg1, interrupt_reason, new_path));
}

}  // namespace

class DownloadItemTest : public testing::Test {
 public:
  class MockObserver : public DownloadItem::Observer {
   public:
    explicit MockObserver(DownloadItem* item)
      : item_(item),
        last_state_(item->GetState()),
        removed_(false),
        destroyed_(false),
        updated_(false),
        interrupt_count_(0),
        resume_count_(0) {
      item_->AddObserver(this);
    }

    virtual ~MockObserver() {
      if (item_) item_->RemoveObserver(this);
    }

    virtual void OnDownloadRemoved(DownloadItem* download) OVERRIDE {
      DVLOG(20) << " " << __FUNCTION__
                << " download = " << download->DebugString(false);
      removed_ = true;
    }

    virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE {
      DVLOG(20) << " " << __FUNCTION__
                << " download = " << download->DebugString(false);
      updated_ = true;
      DownloadItem::DownloadState new_state = download->GetState();
      if (last_state_ == DownloadItem::IN_PROGRESS &&
          new_state == DownloadItem::INTERRUPTED) {
        interrupt_count_++;
      }
      if (last_state_ == DownloadItem::INTERRUPTED &&
          new_state == DownloadItem::IN_PROGRESS) {
        resume_count_++;
      }
      last_state_ = new_state;
    }

    virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE {
      DVLOG(20) << " " << __FUNCTION__
                << " download = " << download->DebugString(false);
    }

    virtual void OnDownloadDestroyed(DownloadItem* download) OVERRIDE {
      DVLOG(20) << " " << __FUNCTION__
                << " download = " << download->DebugString(false);
      destroyed_ = true;
      item_->RemoveObserver(this);
      item_ = NULL;
    }

    bool CheckRemoved() {
      return removed_;
    }

    bool CheckDestroyed() {
      return destroyed_;
    }

    bool CheckUpdated() {
      bool was_updated = updated_;
      updated_ = false;
      return was_updated;
    }

    int GetInterruptCount() {
      return interrupt_count_;
    }

    int GetResumeCount() {
      return resume_count_;
    }

   private:
    DownloadItem* item_;
    DownloadItem::DownloadState last_state_;
    bool removed_;
    bool destroyed_;
    bool updated_;
    int interrupt_count_;
    int resume_count_;
  };

  DownloadItemTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        delegate_() {
  }

  ~DownloadItemTest() {
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
    ui_thread_.DeprecatedGetThreadObject()->message_loop()->RunUntilIdle();
    STLDeleteElements(&allocated_downloads_);
    allocated_downloads_.clear();
  }

  // This class keeps ownership of the created download item; it will
  // be torn down at the end of the test unless DestroyDownloadItem is
  // called.
  DownloadItemImpl* CreateDownloadItem() {
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info_;

    info_.reset(new DownloadCreateInfo());
    static uint32 next_id = DownloadItem::kInvalidId + 1;
    info_->save_info = scoped_ptr<DownloadSaveInfo>(new DownloadSaveInfo());
    info_->save_info->prompt_for_save_location = false;
    info_->url_chain.push_back(GURL());
    info_->etag = "SomethingToSatisfyResumption";

    DownloadItemImpl* download =
        new DownloadItemImpl(
            &delegate_, next_id++, *(info_.get()), net::BoundNetLog());
    allocated_downloads_.insert(download);
    return download;
  }

  // Add DownloadFile to DownloadItem
  MockDownloadFile* AddDownloadFileToDownloadItem(
      DownloadItemImpl* item,
      DownloadItemImplDelegate::DownloadTargetCallback *callback) {
    MockDownloadFile* mock_download_file(new StrictMock<MockDownloadFile>);
    scoped_ptr<DownloadFile> download_file(mock_download_file);
    EXPECT_CALL(*mock_download_file, Initialize(_));
    if (callback) {
      // Save the callback.
      EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _))
          .WillOnce(SaveArg<1>(callback));
    } else {
      // Drop it on the floor.
      EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _));
    }

    scoped_ptr<DownloadRequestHandleInterface> request_handle(
        new NiceMock<MockRequestHandle>);
    item->Start(download_file.Pass(), request_handle.Pass());
    loop_.RunUntilIdle();

    // So that we don't have a function writing to a stack variable
    // lying around if the above failed.
    mock_delegate()->VerifyAndClearExpectations();
    EXPECT_CALL(*mock_delegate(), AssertStateConsistent(_))
        .WillRepeatedly(Return());
    EXPECT_CALL(*mock_delegate(), ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(_, _))
        .WillRepeatedly(Return(true));

    return mock_download_file;
  }

  // Perform the intermediate rename for |item|. The target path for the
  // download will be set to kDummyPath. Returns the MockDownloadFile* that was
  // added to the DownloadItem.
  MockDownloadFile* DoIntermediateRename(DownloadItemImpl* item,
                                         DownloadDangerType danger_type) {
    EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
    EXPECT_TRUE(item->GetTargetFilePath().empty());
    DownloadItemImplDelegate::DownloadTargetCallback callback;
    MockDownloadFile* download_file =
        AddDownloadFileToDownloadItem(item, &callback);
    base::FilePath target_path(kDummyPath);
    base::FilePath intermediate_path(
        target_path.InsertBeforeExtensionASCII("x"));
    EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
        .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                         intermediate_path));
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 danger_type, intermediate_path);
    RunAllPendingInMessageLoops();
    return download_file;
  }

  // Cleanup a download item (specifically get rid of the DownloadFile on it).
  // The item must be in the expected state.
  void CleanupItem(DownloadItemImpl* item,
                   MockDownloadFile* download_file,
                   DownloadItem::DownloadState expected_state) {
    EXPECT_EQ(expected_state, item->GetState());

    if (expected_state == DownloadItem::IN_PROGRESS) {
      EXPECT_CALL(*download_file, Cancel());
      item->Cancel(true);
      loop_.RunUntilIdle();
    }
  }

  // Destroy a previously created download item.
  void DestroyDownloadItem(DownloadItem* item) {
    allocated_downloads_.erase(item);
    delete item;
  }

  void RunAllPendingInMessageLoops() {
    loop_.RunUntilIdle();
  }

  MockDelegate* mock_delegate() {
    return &delegate_;
  }

  void OnDownloadFileAcquired(base::FilePath* return_path,
                              const base::FilePath& path) {
    *return_path = path;
  }

 private:
  base::MessageLoopForUI loop_;
  TestBrowserThread ui_thread_;    // UI thread
  TestBrowserThread file_thread_;  // FILE thread
  StrictMock<MockDelegate> delegate_;
  std::set<DownloadItem*> allocated_downloads_;
};

// Tests to ensure calls that change a DownloadItem generate an update to
// observers.
// State changing functions not tested:
//  void OpenDownload();
//  void ShowDownloadInShell();
//  void CompleteDelayedDownload();
//  set_* mutators

TEST_F(DownloadItemTest, NotificationAfterUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);

  item->DestinationUpdate(kDownloadChunkSize, kDownloadSpeed, std::string());
  ASSERT_TRUE(observer.CheckUpdated());
  EXPECT_EQ(kDownloadSpeed, item->CurrentSpeed());
}

TEST_F(DownloadItemTest, NotificationAfterCancel) {
  DownloadItemImpl* user_cancel = CreateDownloadItem();
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(user_cancel, NULL);
  EXPECT_CALL(*download_file, Cancel());
  MockObserver observer1(user_cancel);

  user_cancel->Cancel(true);
  ASSERT_TRUE(observer1.CheckUpdated());

  DownloadItemImpl* system_cancel = CreateDownloadItem();
  download_file = AddDownloadFileToDownloadItem(system_cancel, NULL);
  EXPECT_CALL(*download_file, Cancel());
  MockObserver observer2(system_cancel);

  system_cancel->Cancel(false);
  ASSERT_TRUE(observer2.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterComplete) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);

  item->OnAllDataSaved(DownloadItem::kEmptyFileHash);
  ASSERT_TRUE(observer.CheckUpdated());

  item->MarkAsComplete();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDownloadedFileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);

  item->OnDownloadedFileRemoved();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterInterrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_CALL(*download_file, Cancel());
  MockObserver observer(item);

  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_,_))
      .Times(0);

  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDestroyed) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);

  DestroyDownloadItem(item);
  ASSERT_TRUE(observer.CheckDestroyed());
}

TEST_F(DownloadItemTest, ContinueAfterInterrupted) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);

  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a continuable interrupt.
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);
  ASSERT_TRUE(observer.CheckUpdated());
  // Should attempt to auto-resume.  Because we don't have a mock WebContents,
  // ResumeInterruptedDownload() will abort early, with another interrupt,
  // which will be ignored.
  ASSERT_EQ(1, observer.GetInterruptCount());
  ASSERT_EQ(0, observer.GetResumeCount());
  RunAllPendingInMessageLoops();

  CleanupItem(item, download_file, DownloadItem::INTERRUPTED);
}

// Same as above, but with a non-continuable interrupt.
TEST_F(DownloadItemTest, RestartAfterInterrupted) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);

  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a restartable interrupt.
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(observer.CheckUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.GetInterruptCount());
  ASSERT_EQ(0, observer.GetResumeCount());
  RunAllPendingInMessageLoops();

  CleanupItem(item, download_file, DownloadItem::INTERRUPTED);
}

// Check we do correct cleanup for RESUME_MODE_INVALID interrupts.
TEST_F(DownloadItemTest, UnresumableInterrupt) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);

  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Fail final rename with unresumable reason.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(base::FilePath(kDummyPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*download_file, Cancel());

  // Complete download to trigger final rename.
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_TRUE(observer.CheckUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.GetInterruptCount());
  ASSERT_EQ(0, observer.GetResumeCount());

  CleanupItem(item, download_file, DownloadItem::INTERRUPTED);
}

TEST_F(DownloadItemTest, LimitRestartsAfterInterrupted) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);

  DownloadItemImpl* item = CreateDownloadItem();
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  MockObserver observer(item);
  MockDownloadFile* mock_download_file(NULL);
  scoped_ptr<DownloadFile> download_file;
  MockRequestHandle* mock_request_handle(NULL);
  scoped_ptr<DownloadRequestHandleInterface> request_handle;
  DownloadItemImplDelegate::DownloadTargetCallback callback;

  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _))
      .WillRepeatedly(SaveArg<1>(&callback));
  for (int i = 0; i < (DownloadItemImpl::kMaxAutoResumeAttempts + 1); ++i) {
    DVLOG(20) << "Loop iteration " << i;

    mock_download_file = new NiceMock<MockDownloadFile>;
    download_file.reset(mock_download_file);
    mock_request_handle = new NiceMock<MockRequestHandle>;
    request_handle.reset(mock_request_handle);

    ON_CALL(*mock_download_file, FullPath())
        .WillByDefault(Return(base::FilePath()));

    // It's too complicated to set up a WebContents instance that would cause
    // the MockDownloadItemDelegate's ResumeInterruptedDownload() function
    // to be callled, so we simply verify that GetWebContents() is called.
    if (i < (DownloadItemImpl::kMaxAutoResumeAttempts - 1)) {
      EXPECT_CALL(*mock_request_handle, GetWebContents())
          .WillRepeatedly(Return(static_cast<WebContents*>(NULL)));
    }

    // Copied key parts of DoIntermediateRename & AddDownloadFileToDownloadItem
    // to allow for holding onto the request handle.
    item->Start(download_file.Pass(), request_handle.Pass());
    RunAllPendingInMessageLoops();
    if (i == 0) {
      // Target determination is only done the first time through.
      base::FilePath target_path(kDummyPath);
      base::FilePath intermediate_path(
          target_path.InsertBeforeExtensionASCII("x"));
      EXPECT_CALL(*mock_download_file, RenameAndUniquify(intermediate_path, _))
          .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                           intermediate_path));
      callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                   DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
      RunAllPendingInMessageLoops();
    }
    ASSERT_EQ(i, observer.GetResumeCount());

    // Use a continuable interrupt.
    item->DestinationObserverAsWeakPtr()->DestinationError(
        DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);

    ASSERT_EQ(i + 1, observer.GetInterruptCount());
    ::testing::Mock::VerifyAndClearExpectations(mock_download_file);
  }

  CleanupItem(item, mock_download_file, DownloadItem::INTERRUPTED);
}

TEST_F(DownloadItemTest, NotificationAfterRemove) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file = AddDownloadFileToDownloadItem(item, NULL);
  EXPECT_CALL(*download_file, Cancel());
  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  MockObserver observer(item);

  item->Remove();
  ASSERT_TRUE(observer.CheckUpdated());
  ASSERT_TRUE(observer.CheckRemoved());
}

TEST_F(DownloadItemTest, NotificationAfterOnContentCheckCompleted) {
  // Setting to NOT_DANGEROUS does not trigger a notification.
  DownloadItemImpl* safe_item = CreateDownloadItem();
  MockObserver safe_observer(safe_item);

  safe_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(safe_observer.CheckUpdated());
  safe_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_TRUE(safe_observer.CheckUpdated());

  // Setting to unsafe url or unsafe file should trigger a notification.
  DownloadItemImpl* unsafeurl_item =
      CreateDownloadItem();
  MockObserver unsafeurl_observer(unsafeurl_item);

  unsafeurl_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(unsafeurl_observer.CheckUpdated());
  unsafeurl_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
  EXPECT_TRUE(unsafeurl_observer.CheckUpdated());

  unsafeurl_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafeurl_observer.CheckUpdated());

  DownloadItemImpl* unsafefile_item =
      CreateDownloadItem();
  MockObserver unsafefile_observer(unsafefile_item);

  unsafefile_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(unsafefile_observer.CheckUpdated());
  unsafefile_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_TRUE(unsafefile_observer.CheckUpdated());

  unsafefile_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafefile_observer.CheckUpdated());
}

// DownloadItemImpl::OnDownloadTargetDetermined will schedule a task to run
// DownloadFile::Rename(). Once the rename
// completes, DownloadItemImpl receives a notification with the new file
// name. Check that observers are updated when the new filename is available and
// not before.
TEST_F(DownloadItemTest, NotificationAfterOnDownloadTargetDetermined) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  MockObserver observer(item);
  base::FilePath target_path(kDummyPath);
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      target_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));

  // Currently, a notification would be generated if the danger type is anything
  // other than NOT_DANGEROUS.
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  EXPECT_FALSE(observer.CheckUpdated());
  RunAllPendingInMessageLoops();
  EXPECT_TRUE(observer.CheckUpdated());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterTogglePause) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockObserver observer(item);
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  scoped_ptr<DownloadFile> download_file(mock_download_file);
  scoped_ptr<DownloadRequestHandleInterface> request_handle(
      new NiceMock<MockRequestHandle>);

  EXPECT_CALL(*mock_download_file, Initialize(_));
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(_, _));
  item->Start(download_file.Pass(), request_handle.Pass());

  item->Pause();
  ASSERT_TRUE(observer.CheckUpdated());

  ASSERT_TRUE(item->IsPaused());

  item->Resume();
  ASSERT_TRUE(observer.CheckUpdated());

  RunAllPendingInMessageLoops();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, DisplayName) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  base::FilePath target_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            item->GetFileNameToReportUser().value());
  EXPECT_CALL(*download_file, RenameAndUniquify(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       intermediate_path));
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  EXPECT_EQ(FILE_PATH_LITERAL("foo.bar"),
            item->GetFileNameToReportUser().value());
  item->SetDisplayName(base::FilePath(FILE_PATH_LITERAL("new.name")));
  EXPECT_EQ(FILE_PATH_LITERAL("new.name"),
            item->GetFileNameToReportUser().value());
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Test to make sure that Start method calls DF initialize properly.
TEST_F(DownloadItemTest, Start) {
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  scoped_ptr<DownloadFile> download_file(mock_download_file);
  DownloadItemImpl* item = CreateDownloadItem();
  EXPECT_CALL(*mock_download_file, Initialize(_));
  scoped_ptr<DownloadRequestHandleInterface> request_handle(
      new NiceMock<MockRequestHandle>);
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _));
  item->Start(download_file.Pass(), request_handle.Pass());
  RunAllPendingInMessageLoops();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

// Test that the delegate is invoked after the download file is renamed.
TEST_F(DownloadItemTest, CallbackAfterRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  base::FilePath final_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(final_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       final_path));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
}

// Test that the delegate is invoked after the download file is renamed and the
// download item is in an interrupted state.
TEST_F(DownloadItemTest, CallbackAfterInterruptedRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  base::FilePath final_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
}

TEST_F(DownloadItemTest, Interrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  const DownloadInterruptReason reason(
      DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);

  // Confirm interrupt sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(reason);
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(reason, item->GetLastReason());

  // Cancel should kill it.
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, item->GetLastReason());
}

// Destination errors that occur before the intermediate rename shouldn't cause
// the download to be marked as interrupted until after the intermediate rename.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Restart) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

// As above. But if the download can be resumed by continuing, then the
// intermediate path should be retained when the download is interrupted after
// the intermediate rename succeeds.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Continue) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath(new_intermediate_path)));
  EXPECT_CALL(*download_file, Detach());

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

// As above. If the intermediate rename fails, then the interrupt reason should
// be set to the destination error and the intermediate path should be empty.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Failed) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file =
      AddDownloadFileToDownloadItem(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(base::FilePath(kDummyPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item->GetLastReason());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

TEST_F(DownloadItemTest, Canceled) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file = AddDownloadFileToDownloadItem(item, NULL);

  // Confirm cancel sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
}

TEST_F(DownloadItemTest, FileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();

  EXPECT_FALSE(item->GetFileExternallyRemoved());
  item->OnDownloadedFileRemoved();
  EXPECT_TRUE(item->GetFileExternallyRemoved());
}

TEST_F(DownloadItemTest, DestinationUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  MockObserver observer(item);

  EXPECT_EQ(0l, item->CurrentSpeed());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_EQ(0l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_FALSE(observer.CheckUpdated());
  item->SetTotalBytes(100l);
  EXPECT_EQ(100l, item->GetTotalBytes());

  as_observer->DestinationUpdate(10, 20, "deadbeef");
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ("deadbeef", item->GetHashState());
  EXPECT_EQ(10l, item->GetReceivedBytes());
  EXPECT_EQ(100l, item->GetTotalBytes());
  EXPECT_TRUE(observer.CheckUpdated());

  as_observer->DestinationUpdate(200, 20, "livebeef");
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ("livebeef", item->GetHashState());
  EXPECT_EQ(200l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, DestinationError) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  MockObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, item->GetLastReason());
  EXPECT_FALSE(observer.CheckUpdated());

  EXPECT_CALL(*download_file, Cancel());
  as_observer->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_TRUE(observer.CheckUpdated());
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
            item->GetLastReason());
}

TEST_F(DownloadItemTest, DestinationCompleted) {
  DownloadItemImpl* item = CreateDownloadItem();
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  MockObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_FALSE(item->AllDataSaved());
  EXPECT_FALSE(observer.CheckUpdated());

  as_observer->DestinationUpdate(10, 20, "deadbeef");
  EXPECT_TRUE(observer.CheckUpdated());
  EXPECT_FALSE(observer.CheckUpdated()); // Confirm reset.
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_EQ("deadbeef", item->GetHashState());
  EXPECT_FALSE(item->AllDataSaved());

  as_observer->DestinationCompleted("livebeef");
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(observer.CheckUpdated());
  EXPECT_EQ("livebeef", item->GetHash());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_TRUE(item->AllDataSaved());
}

TEST_F(DownloadItemTest, EnabledActionsForNormalDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // InProgress
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());

  // Complete
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForTemporaryDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  item->SetIsTemporary(true);

  // InProgress Temporary
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  ASSERT_TRUE(item->IsTemporary());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());

  // Complete Temporary
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForInterruptedDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForCancelledDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

// Test various aspects of the delegate completion blocker.

// Just allowing completion.
TEST_F(DownloadItemTest, CompleteDelegate_ReturnTrue) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file, RenameAndAnnotate(base::FilePath(kDummyPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion.
TEST_F(DownloadItemTest, CompleteDelegate_BlockOnce) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file, RenameAndAnnotate(base::FilePath(kDummyPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Delay and set danger.
TEST_F(DownloadItemTest, CompleteDelegate_SetDanger) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_FALSE(item->IsDangerous());
  item->OnContentCheckCompleted(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  // Make sure the download doesn't complete until we've validated it.
  EXPECT_CALL(*download_file, RenameAndAnnotate(base::FilePath(kDummyPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  item->ValidateDangerousDownload();
  EXPECT_EQ(DOWNLOAD_DANGER_TYPE_USER_VALIDATED, item->GetDangerType());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion twice.
TEST_F(DownloadItemTest, CompleteDelegate_BlockTwice) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file, RenameAndAnnotate(base::FilePath(kDummyPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

TEST_F(DownloadItemTest, StealDangerousDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  ASSERT_TRUE(item->IsDangerous());
  base::FilePath full_path(FILE_PATH_LITERAL("foo.txt"));
  base::FilePath returned_path;

  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(full_path));
  EXPECT_CALL(*download_file, Detach());
  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_EQ(full_path, returned_path);
}

TEST_F(DownloadItemTest, StealInterruptedDangerousDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  base::FilePath returned_path;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  base::FilePath full_path = item->GetFullPath();
  EXPECT_FALSE(full_path.empty());
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(full_path));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_TRUE(item->IsDangerous());

  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_EQ(full_path, returned_path);
}

TEST_F(DownloadItemTest, StealInterruptedNonResumableDangerousDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  base::FilePath returned_path;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(item->IsDangerous());

  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_TRUE(returned_path.empty());
}

TEST(MockDownloadItem, Compiles) {
  MockDownloadItem mock_item;
}

}  // namespace content
