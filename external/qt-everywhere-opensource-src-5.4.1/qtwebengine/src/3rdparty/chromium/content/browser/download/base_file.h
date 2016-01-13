// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "url/gurl.h"

namespace crypto {
class SecureHash;
}

namespace content {

// File being downloaded and saved to disk. This is a base class
// for DownloadFile and SaveFile, which keep more state information.
class CONTENT_EXPORT BaseFile {
 public:
  // May be constructed on any thread.  All other routines (including
  // destruction) must occur on the FILE thread.
  BaseFile(const base::FilePath& full_path,
           const GURL& source_url,
           const GURL& referrer_url,
           int64 received_bytes,
           bool calculate_hash,
           const std::string& hash_state,
           base::File file,
           const net::BoundNetLog& bound_net_log);
  virtual ~BaseFile();

  // Returns DOWNLOAD_INTERRUPT_REASON_NONE on success, or a
  // DownloadInterruptReason on failure.  |default_directory| specifies the
  // directory to create the temporary file in if |full_path()| is empty. If
  // |default_directory| and |full_path()| are empty, then a temporary file will
  // be created in the default download location as determined by
  // ContentBrowserClient.
  DownloadInterruptReason Initialize(const base::FilePath& default_directory);

  // Write a new chunk of data to the file. Returns a DownloadInterruptReason
  // indicating the result of the operation.
  DownloadInterruptReason AppendDataToFile(const char* data, size_t data_len);

  // Rename the download file. Returns a DownloadInterruptReason indicating the
  // result of the operation.
  virtual DownloadInterruptReason Rename(const base::FilePath& full_path);

  // Detach the file so it is not deleted on destruction.
  virtual void Detach();

  // Abort the download and automatically close the file.
  void Cancel();

  // Indicate that the download has finished. No new data will be received.
  void Finish();

  // Set the client guid which will be used to identify the app to the
  // system AV scanning function. Should be called before
  // AnnotateWithSourceInformation() to take effect.
  void SetClientGuid(const std::string& guid);

  // Informs the OS that this file came from the internet. Returns a
  // DownloadInterruptReason indicating the result of the operation.
  // Note: SetClientGuid() should be called before this function on
  // Windows to ensure the correct app client ID is available.
  DownloadInterruptReason AnnotateWithSourceInformation();

  base::FilePath full_path() const { return full_path_; }
  bool in_progress() const { return file_.IsValid(); }
  int64 bytes_so_far() const { return bytes_so_far_; }

  // Fills |hash| with the hash digest for the file.
  // Returns true if digest is successfully calculated.
  virtual bool GetHash(std::string* hash);

  // Returns the current (intermediate) state of the hash as a byte string.
  virtual std::string GetHashState();

  // Returns true if the given hash is considered empty.  An empty hash is
  // a string of size crypto::kSHA256Length that contains only zeros (initial
  // value for the hash).
  static bool IsEmptyHash(const std::string& hash);

  virtual std::string DebugString() const;

 private:
  friend class BaseFileTest;
  FRIEND_TEST_ALL_PREFIXES(BaseFileTest, IsEmptyHash);

  // Creates and opens the file_ if it is NULL.
  DownloadInterruptReason Open();

  // Closes and resets file_.
  void Close();

  // Resets file_.
  void ClearFile();

  // Platform specific method that moves a file to a new path and adjusts the
  // security descriptor / permissions on the file to match the defaults for the
  // new directory.
  DownloadInterruptReason MoveFileAndAdjustPermissions(
      const base::FilePath& new_path);

  // Split out from CurrentSpeed to enable testing.
  int64 CurrentSpeedAtTime(base::TimeTicks current_time) const;

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |error| and passes error
  // on through, converting to a |DownloadInterruptReason|.
  DownloadInterruptReason LogNetError(const char* operation, net::Error error);

  // Log the system error in |os_error| and converts it into a
  // |DownloadInterruptReason|.
  DownloadInterruptReason LogSystemError(const char* operation,
                                         logging::SystemErrorCode os_error);

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |os_error| and |reason|.
  // Returns |reason|.
  DownloadInterruptReason LogInterruptReason(
      const char* operation, int os_error,
      DownloadInterruptReason reason);

  static const unsigned char kEmptySha256Hash[crypto::kSHA256Length];

  // Full path to the file including the file name.
  base::FilePath full_path_;

  // Source URL for the file being downloaded.
  GURL source_url_;

  // The URL where the download was initiated.
  GURL referrer_url_;

  std::string client_guid_;

  // OS file for writing
  base::File file_;

  // Amount of data received up so far, in bytes.
  int64 bytes_so_far_;

  // Start time for calculating speed.
  base::TimeTicks start_tick_;

  // Indicates if hash should be calculated for the file.
  bool calculate_hash_;

  // Used to calculate hash for the file when calculate_hash_
  // is set.
  scoped_ptr<crypto::SecureHash> secure_hash_;

  unsigned char sha256_hash_[crypto::kSHA256Length];

  // Indicates that this class no longer owns the associated file, and so
  // won't delete it on destruction.
  bool detached_;

  net::BoundNetLog bound_net_log_;

  DISALLOW_COPY_AND_ASSIGN(BaseFile);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_
