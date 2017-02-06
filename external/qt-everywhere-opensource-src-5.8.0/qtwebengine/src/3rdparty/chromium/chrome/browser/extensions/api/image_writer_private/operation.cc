// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/google/zip_reader.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

const int kMD5BufferSize = 1024;
#if defined(OS_CHROMEOS)
// Chrome OS only has a 1 GB temporary partition.  This is too small to hold our
// unzipped image. Fortunately we mount part of the temporary partition under
// /var/tmp.
const char kChromeOSTempRoot[] = "/var/tmp";
#endif

#if !defined(OS_CHROMEOS)
static base::LazyInstance<scoped_refptr<ImageWriterUtilityClient> >
    g_utility_client = LAZY_INSTANCE_INITIALIZER;
#endif

Operation::Operation(base::WeakPtr<OperationManager> manager,
                     const ExtensionId& extension_id,
                     const std::string& device_path)
    : manager_(manager),
      extension_id_(extension_id),
#if defined(OS_WIN)
      device_path_(base::FilePath::FromUTF8Unsafe(device_path)),
#else
      device_path_(device_path),
#endif
      stage_(image_writer_api::STAGE_UNKNOWN),
      progress_(0),
      zip_reader_(new zip::ZipReader) {
}

Operation::~Operation() {}

void Operation::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  stage_ = image_writer_api::STAGE_NONE;

  CleanUp();
}

void Operation::Abort() {
  Error(error::kAborted);
}

int Operation::GetProgress() {
  return progress_;
}

image_writer_api::Stage Operation::GetStage() {
  return stage_;
}

#if !defined(OS_CHROMEOS)
// static
void Operation::SetUtilityClientForTesting(
    scoped_refptr<ImageWriterUtilityClient> client) {
  g_utility_client.Get() = client;
}
#endif

void Operation::Start() {
#if defined(OS_CHROMEOS)
  if (!temp_dir_.CreateUniqueTempDirUnderPath(
           base::FilePath(kChromeOSTempRoot))) {
#else
  if (!temp_dir_.CreateUniqueTempDir()) {
#endif
    Error(error::kTempDirError);
    return;
  }

  AddCleanUpFunction(
      base::Bind(base::IgnoreResult(&base::ScopedTempDir::Delete),
                 base::Unretained(&temp_dir_)));

  StartImpl();
}

void Operation::Unzip(const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (IsCancelled()) {
    return;
  }

  if (image_path_.Extension() != FILE_PATH_LITERAL(".zip")) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
    return;
  }

  SetStage(image_writer_api::STAGE_UNZIP);

  if (!(zip_reader_->Open(image_path_) && zip_reader_->AdvanceToNextEntry() &&
        zip_reader_->OpenCurrentEntryInZip())) {
    Error(error::kUnzipGenericError);
    return;
  }

  if (zip_reader_->HasMore()) {
    Error(error::kUnzipInvalidArchive);
    return;
  }

  // Create a new target to unzip to.  The original file is opened by the
  // zip_reader_.
  zip::ZipReader::EntryInfo* entry_info = zip_reader_->current_entry_info();
  if (entry_info) {
    image_path_ = temp_dir_.path().Append(entry_info->file_path().BaseName());
  } else {
    Error(error::kTempDirError);
    return;
  }

  zip_reader_->ExtractCurrentEntryToFilePathAsync(
      image_path_,
      base::Bind(&Operation::CompleteAndContinue, this, continuation),
      base::Bind(&Operation::OnUnzipFailure, this),
      base::Bind(&Operation::OnUnzipProgress,
                 this,
                 zip_reader_->current_entry_info()->original_size()));
}

void Operation::Finish() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE, base::Bind(&Operation::Finish, this));
    return;
  }

  CleanUp();

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnComplete, manager_, extension_id_));
}

void Operation::Error(const std::string& error_message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Error, this, error_message));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnError,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_,
                 error_message));

  CleanUp();
}

void Operation::SetProgress(int progress) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetProgress,
                   this,
                   progress));
    return;
  }

  if (progress <= progress_) {
    return;
  }

  if (IsCancelled()) {
    return;
  }

  progress_ = progress;

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&OperationManager::OnProgress,
                                     manager_,
                                     extension_id_,
                                     stage_,
                                     progress_));
}

void Operation::SetStage(image_writer_api::Stage stage) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetStage,
                   this,
                   stage));
    return;
  }

  if (IsCancelled()) {
    return;
  }

  stage_ = stage;
  progress_ = 0;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnProgress,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_));
}

bool Operation::IsCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  return stage_ == image_writer_api::STAGE_NONE;
}

void Operation::AddCleanUpFunction(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  cleanup_functions_.push_back(callback);
}

void Operation::CompleteAndContinue(const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  SetProgress(kProgressComplete);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

#if !defined(OS_CHROMEOS)
void Operation::StartUtilityClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (g_utility_client.Get().get()) {
    image_writer_client_ = g_utility_client.Get();
    return;
  }
  if (!image_writer_client_.get()) {
    image_writer_client_ = new ImageWriterUtilityClient();
    AddCleanUpFunction(base::Bind(&Operation::StopUtilityClient, this));
  }
}

void Operation::StopUtilityClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ImageWriterUtilityClient::Shutdown, image_writer_client_));
}

void Operation::WriteImageProgress(int64_t total_bytes, int64_t curr_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (IsCancelled()) {
    return;
  }

  int progress = kProgressComplete * curr_bytes / total_bytes;

  if (progress > GetProgress()) {
    SetProgress(progress);
  }
}
#endif

void Operation::GetMD5SumOfFile(
    const base::FilePath& file_path,
    int64_t file_size,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(const std::string&)>& callback) {
  if (IsCancelled()) {
    return;
  }

  base::MD5Init(&md5_context_);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    Error(error::kImageOpenError);
    return;
  }

  if (file_size <= 0) {
    file_size = file.GetLength();
    if (file_size < 0) {
      Error(error::kImageOpenError);
      return;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&Operation::MD5Chunk, this, Passed(std::move(file)), 0,
                 file_size, progress_offset, progress_scale, callback));
}

void Operation::MD5Chunk(
    base::File file,
    int64_t bytes_processed,
    int64_t bytes_total,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(const std::string&)>& callback) {
  if (IsCancelled())
    return;

  CHECK_LE(bytes_processed, bytes_total);

  std::unique_ptr<char[]> buffer(new char[kMD5BufferSize]);
  int read_size = std::min(bytes_total - bytes_processed,
                           static_cast<int64_t>(kMD5BufferSize));

  if (read_size == 0) {
    // Nothing to read, we are done.
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context_);
    callback.Run(base::MD5DigestToBase16(digest));
  } else {
    int len = file.Read(bytes_processed, buffer.get(), read_size);

    if (len == read_size) {
      // Process data.
      base::MD5Update(&md5_context_, base::StringPiece(buffer.get(), len));
      int percent_curr =
          ((bytes_processed + len) * progress_scale) / bytes_total +
          progress_offset;
      SetProgress(percent_curr);

      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&Operation::MD5Chunk, this, Passed(std::move(file)),
                     bytes_processed + len, bytes_total, progress_offset,
                     progress_scale, callback));
      // Skip closing the file.
      return;
    } else {
      // We didn't read the bytes we expected.
      Error(error::kHashReadError);
    }
  }
}

void Operation::OnUnzipFailure() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  Error(error::kUnzipGenericError);
}

void Operation::OnUnzipProgress(int64_t total_bytes, int64_t progress_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  int progress_percent = kProgressComplete * progress_bytes / total_bytes;
  SetProgress(progress_percent);
}

void Operation::CleanUp() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  for (std::vector<base::Closure>::iterator it = cleanup_functions_.begin();
       it != cleanup_functions_.end();
       ++it) {
    it->Run();
  }
  cleanup_functions_.clear();
}

}  // namespace image_writer
}  // namespace extensions
