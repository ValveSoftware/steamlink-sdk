// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/quota/open_file_handle.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

using fileapi::kFileSystemTypeTemporary;
using fileapi::OpenFileHandle;
using fileapi::QuotaReservation;
using fileapi::QuotaReservationManager;

namespace content {

namespace {

const char kOrigin[] = "http://example.com";
const fileapi::FileSystemType kType = kFileSystemTypeTemporary;
const int64 kInitialFileSize = 1;

typedef QuotaReservationManager::ReserveQuotaCallback ReserveQuotaCallback;

int64 GetFileSize(const base::FilePath& path) {
  int64 size = 0;
  base::GetFileSize(path, &size);
  return size;
}

void SetFileSize(const base::FilePath& path, int64 size) {
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.SetLength(size));
}

class FakeBackend : public QuotaReservationManager::QuotaBackend {
 public:
  FakeBackend()
      : on_memory_usage_(kInitialFileSize),
        on_disk_usage_(kInitialFileSize) {}
  virtual ~FakeBackend() {}

  virtual void ReserveQuota(const GURL& origin,
                            fileapi::FileSystemType type,
                            int64 delta,
                            const ReserveQuotaCallback& callback) OVERRIDE {
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ += delta;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(callback), base::File::FILE_OK, delta));
  }

  virtual void ReleaseReservedQuota(const GURL& origin,
                                    fileapi::FileSystemType type,
                                    int64 size) OVERRIDE {
    EXPECT_LE(0, size);
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ -= size;
  }

  virtual void CommitQuotaUsage(const GURL& origin,
                                fileapi::FileSystemType type,
                                int64 delta) OVERRIDE {
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_disk_usage_ += delta;
    on_memory_usage_ += delta;
  }

  virtual void IncrementDirtyCount(const GURL& origin,
                                   fileapi::FileSystemType type) OVERRIDE {}
  virtual void DecrementDirtyCount(const GURL& origin,
                                   fileapi::FileSystemType type) OVERRIDE {}

  int64 on_memory_usage() { return on_memory_usage_; }
  int64 on_disk_usage() { return on_disk_usage_; }

 private:
  int64 on_memory_usage_;
  int64 on_disk_usage_;

  DISALLOW_COPY_AND_ASSIGN(FakeBackend);
};

class FakeWriter {
 public:
  explicit FakeWriter(scoped_ptr<OpenFileHandle> handle)
      : handle_(handle.Pass()),
        path_(handle_->platform_path()),
        max_written_offset_(handle_->GetEstimatedFileSize()),
        append_mode_write_amount_(0),
        dirty_(false) {
  }

  ~FakeWriter() {
    if (handle_)
      EXPECT_FALSE(dirty_);
  }

  int64 Truncate(int64 length) {
    int64 consumed = 0;

    if (max_written_offset_ < length) {
      consumed = length - max_written_offset_;
      max_written_offset_ = length;
    }
    SetFileSize(path_, length);
    return consumed;
  }

  int64 Write(int64 max_offset) {
    dirty_ = true;

    int64 consumed = 0;
    if (max_written_offset_ < max_offset) {
      consumed = max_offset - max_written_offset_;
      max_written_offset_ = max_offset;
    }
    if (GetFileSize(path_) < max_offset)
      SetFileSize(path_, max_offset);
    return consumed;
  }

  int64 Append(int64 amount) {
    dirty_ = true;
    append_mode_write_amount_ += amount;
    SetFileSize(path_, GetFileSize(path_) + amount);
    return amount;
  }

  void ReportUsage() {
    handle_->UpdateMaxWrittenOffset(max_written_offset_);
    handle_->AddAppendModeWriteAmount(append_mode_write_amount_);
    max_written_offset_ = handle_->GetEstimatedFileSize();
    append_mode_write_amount_ = 0;
    dirty_ = false;
  }

  void ClearWithoutUsageReport() {
    handle_.reset();
  }

 private:
  scoped_ptr<OpenFileHandle> handle_;
  base::FilePath path_;
  int64 max_written_offset_;
  int64 append_mode_write_amount_;
  bool dirty_;
};

void ExpectSuccess(bool* done, base::File::Error error) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(base::File::FILE_OK, error);
}

void RefreshReservation(QuotaReservation* reservation, int64 size) {
  DCHECK(reservation);

  bool done = false;
  reservation->RefreshReservation(size, base::Bind(&ExpectSuccess, &done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(done);
}

}  // namespace

class QuotaReservationManagerTest : public testing::Test {
 public:
  QuotaReservationManagerTest() {}
  virtual ~QuotaReservationManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
    file_path_ = work_dir_.path().Append(FILE_PATH_LITERAL("hoge"));
    SetFileSize(file_path_, kInitialFileSize);

    scoped_ptr<QuotaReservationManager::QuotaBackend> backend(new FakeBackend);
    reservation_manager_.reset(new QuotaReservationManager(backend.Pass()));
  }

  virtual void TearDown() OVERRIDE {
    reservation_manager_.reset();
  }

  FakeBackend* fake_backend() {
    return static_cast<FakeBackend*>(reservation_manager_->backend_.get());
  }

  QuotaReservationManager* reservation_manager() {
    return reservation_manager_.get();
  }

  const base::FilePath& file_path() const {
    return file_path_;
  }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir work_dir_;
  base::FilePath file_path_;
  scoped_ptr<QuotaReservationManager> reservation_manager_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservationManagerTest);
};

TEST_F(QuotaReservationManagerTest, BasicTest) {
  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);

  {
    RefreshReservation(reservation.get(), 10 + 20 + 3);
    int64 cached_reserved_quota = reservation->remaining_quota();
    FakeWriter writer(reservation->GetOpenFileHandle(file_path()));

    cached_reserved_quota -= writer.Write(kInitialFileSize + 10);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer.Append(20);
    EXPECT_LE(0, cached_reserved_quota);

    writer.ReportUsage();
  }

  EXPECT_EQ(3, reservation->remaining_quota());
  EXPECT_EQ(kInitialFileSize + 10 + 20, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_disk_usage());
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 3, fake_backend()->on_memory_usage());

  {
    RefreshReservation(reservation.get(), 5);
    FakeWriter writer(reservation->GetOpenFileHandle(file_path()));

    EXPECT_EQ(0, writer.Truncate(3));

    writer.ReportUsage();
  }

  EXPECT_EQ(5, reservation->remaining_quota());
  EXPECT_EQ(3, GetFileSize(file_path()));
  EXPECT_EQ(3, fake_backend()->on_disk_usage());
  EXPECT_EQ(3 + 5, fake_backend()->on_memory_usage());

  reservation = NULL;

  EXPECT_EQ(3, fake_backend()->on_memory_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleWriter) {
  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);

  {
    RefreshReservation(reservation.get(), 10 + 20 + 30 + 40 + 5);
    int64 cached_reserved_quota = reservation->remaining_quota();
    FakeWriter writer1(reservation->GetOpenFileHandle(file_path()));
    FakeWriter writer2(reservation->GetOpenFileHandle(file_path()));
    FakeWriter writer3(reservation->GetOpenFileHandle(file_path()));

    cached_reserved_quota -= writer1.Write(kInitialFileSize + 10);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer2.Write(kInitialFileSize + 20);
    cached_reserved_quota -= writer3.Append(30);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer3.Append(40);
    EXPECT_LE(0, cached_reserved_quota);

    writer1.ReportUsage();
    writer2.ReportUsage();
    writer3.ReportUsage();
  }

  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 30 + 40 + 5,
            fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, fake_backend()->on_disk_usage());

  reservation = NULL;

  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, fake_backend()->on_disk_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleClient) {
  scoped_refptr<QuotaReservation> reservation1 =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);
  RefreshReservation(reservation1, 10);
  int64 cached_reserved_quota1 = reservation1->remaining_quota();

  scoped_refptr<QuotaReservation> reservation2 =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);
  RefreshReservation(reservation2, 20);
  int64 cached_reserved_quota2 = reservation2->remaining_quota();

  scoped_ptr<FakeWriter> writer1(
      new FakeWriter(reservation1->GetOpenFileHandle(file_path())));

  scoped_ptr<FakeWriter> writer2(
      new FakeWriter(reservation2->GetOpenFileHandle(file_path())));

  cached_reserved_quota1 -= writer1->Write(kInitialFileSize + 10);
  EXPECT_LE(0, cached_reserved_quota1);

  cached_reserved_quota2 -= writer2->Append(20);
  EXPECT_LE(0, cached_reserved_quota2);

  writer1->ReportUsage();
  RefreshReservation(reservation1.get(), 2);
  cached_reserved_quota1 = reservation1->remaining_quota();

  writer2->ReportUsage();
  RefreshReservation(reservation2.get(), 3);
  cached_reserved_quota2 = reservation2->remaining_quota();

  writer1.reset();
  writer2.reset();

  EXPECT_EQ(kInitialFileSize + 10 + 20, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 2 + 3,
            fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_disk_usage());

  reservation1 = NULL;
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 3, fake_backend()->on_memory_usage());

  reservation2 = NULL;
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_memory_usage());
}

TEST_F(QuotaReservationManagerTest, ClientCrash) {
  scoped_refptr<QuotaReservation> reservation1 =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);
  RefreshReservation(reservation1.get(), 15);

  scoped_refptr<QuotaReservation> reservation2 =
      reservation_manager()->CreateReservation(GURL(kOrigin), kType);
  RefreshReservation(reservation2.get(), 20);

  {
    FakeWriter writer(reservation1->GetOpenFileHandle(file_path()));

    writer.Write(kInitialFileSize + 10);

    reservation1->OnClientCrash();
    writer.ClearWithoutUsageReport();
  }
  reservation1 = NULL;

  EXPECT_EQ(kInitialFileSize + 10, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 15 + 20, fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 10, fake_backend()->on_disk_usage());

  reservation2 = NULL;
  EXPECT_EQ(kInitialFileSize + 10, fake_backend()->on_memory_usage());
}

}  // namespace content
