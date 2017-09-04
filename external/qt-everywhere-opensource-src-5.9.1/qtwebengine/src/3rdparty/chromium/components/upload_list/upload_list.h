// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
#define COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
class TaskRunner;
}

// Loads and parses an upload list text file of the format
// upload_time,upload_id[,local_id[,capture_time[,state]]]
// upload_time,upload_id[,local_id[,capture_time[,state]]]
// etc.
// where each line represents an upload. |upload_time| and |capture_time| are in
// Unix time. |state| is an int in the range of UploadInfo::State. Must be used
// from the UI thread. The loading and parsing is done on a blocking pool task
// runner. A line may or may not contain |local_id|, |capture_time|, and
// |state|.
class UploadList : public base::RefCountedThreadSafe<UploadList> {
 public:
  struct UploadInfo {
    enum class State {
      NotUploaded = 0,
      Pending,
      Pending_UserRequested,
      Uploaded,
    };

    UploadInfo(const std::string& upload_id,
               const base::Time& upload_time,
               const std::string& local_id,
               const base::Time& capture_time,
               State state);
    // Constructor for locally stored data.
    UploadInfo(const std::string& local_id,
               const base::Time& capture_time,
               State state,
               const base::string16& file_size);
    UploadInfo(const std::string& upload_id, const base::Time& upload_time);
    UploadInfo(const UploadInfo& upload_info);
    ~UploadInfo();

    // These fields are only valid when |state| == UploadInfo::State::Uploaded.
    std::string upload_id;
    base::Time upload_time;

    // ID for locally stored data that may or may not be uploaded.
    std::string local_id;

    // The time the data was captured. This is useful if the data is stored
    // locally when captured and uploaded at a later time.
    base::Time capture_time;

    State state;

    // Formatted file size for locally stored data.
    base::string16 file_size;
  };

  class Delegate {
   public:
    // Invoked when the upload list has been loaded. Will be called on the
    // UI thread.
    virtual void OnUploadListAvailable() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a new upload list with the given callback delegate.
  UploadList(Delegate* delegate,
             const base::FilePath& upload_log_path,
             scoped_refptr<base::TaskRunner> task_runner);

  // Starts loading the upload list. OnUploadListAvailable will be called when
  // loading is complete.
  void LoadUploadListAsynchronously();

  // Asynchronously requests a user triggered upload.
  void RequestSingleCrashUploadAsync(const std::string& local_id);

  // Clears the delegate, so that any outstanding asynchronous load will not
  // call the delegate on completion.
  void ClearDelegate();

  // Populates |uploads| with the |max_count| most recent uploads,
  // in reverse chronological order.
  // Must be called only after OnUploadListAvailable has been called.
  void GetUploads(size_t max_count, std::vector<UploadInfo>* uploads);

 protected:
  virtual ~UploadList();

  // Reads the upload log and stores the entries in |uploads|.
  virtual void LoadUploadList(std::vector<UploadInfo>* uploads);

  // Requests a user triggered upload for a crash report with a given id.
  virtual void RequestSingleCrashUpload(const std::string& local_id);

  const base::FilePath& upload_log_path() const;

 private:
  friend class base::RefCountedThreadSafe<UploadList>;

  // Manages the background thread work for LoadUploadListAsynchronously().
  void PerformLoadAndNotifyDelegate(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Calls the delegate's callback method, if there is a delegate. Stores
  // the newly loaded |uploads| into |uploads_| on the delegate's task runner.
  void SetUploadsAndNotifyDelegate(std::vector<UploadInfo> uploads);

  // Parses upload log lines, converting them to UploadInfo entries.
  void ParseLogEntries(const std::vector<std::string>& log_entries,
                       std::vector<UploadInfo>* uploads);

  // Ensures that this class' thread unsafe state is only accessed from the
  // sequence that owns this UploadList.
  base::SequenceChecker sequence_checker_;

  std::vector<UploadInfo> uploads_;

  Delegate* delegate_;

  const base::FilePath upload_log_path_;

  const scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UploadList);
};

#endif  // COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
