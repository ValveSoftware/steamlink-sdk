// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_BLOB_SCOPED_FILE_H_
#define WEBKIT_COMMON_BLOB_SCOPED_FILE_H_

#include <map>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/move.h"
#include "webkit/common/webkit_storage_common_export.h"

namespace base {
class TaskRunner;
}

namespace webkit_blob {

// A scoped reference for a FilePath that can optionally schedule the file
// to be deleted and/or to notify a consumer when it is going to be scoped out.
// This class supports move semantics, i.e. consumers can call Pass() to
// pass the ownership of ScopedFile.
//
// TODO(kinuko): Probably this can be moved under base or somewhere more
// common place.
class WEBKIT_STORAGE_COMMON_EXPORT ScopedFile {
  // To support destructive assignment from an l-value assignment.
  // This provides Pass() method which creates an r-value for the current
  // instance. (See base/move.h for details)
  MOVE_ONLY_TYPE_FOR_CPP_03(ScopedFile, RValue)

 public:
  typedef base::Callback<void(const base::FilePath&)> ScopeOutCallback;
  typedef std::pair<ScopeOutCallback, scoped_refptr<base::TaskRunner> >
      ScopeOutCallbackPair;
  typedef std::vector<ScopeOutCallbackPair> ScopeOutCallbackList;

  enum ScopeOutPolicy {
    DELETE_ON_SCOPE_OUT,
    DONT_DELETE_ON_SCOPE_OUT,
  };

  ScopedFile();

  // |file_task_runner| is used to schedule a file deletion if |policy|
  // is DELETE_ON_SCOPE_OUT.
  ScopedFile(const base::FilePath& path,
             ScopeOutPolicy policy,
             base::TaskRunner* file_task_runner);

  // Move constructor and operator. The data of r-value will be transfered
  // in a destructive way. (See base/move.h)
  ScopedFile(RValue other);
  ScopedFile& operator=(RValue rhs) {
    MoveFrom(*rhs.object);
    return *this;
  }

  ~ScopedFile();

  // The |callback| is fired on |callback_runner| when the final reference
  // of this instance is released.
  // If release policy is DELETE_ON_SCOPE_OUT the
  // callback task(s) is/are posted before the deletion is scheduled.
  void AddScopeOutCallback(const ScopeOutCallback& callback,
                           base::TaskRunner* callback_runner);

  // The full file path.
  const base::FilePath& path() const { return path_; }

  // Releases the file. After calling this, this instance will hold
  // an empty file path and scoping out won't make any file deletion
  // or callback dispatch. (If an owned pointer is attached to any of
  // callbacks the pointer will be deleted.)
  base::FilePath Release();

  void Reset();

 private:
  // Performs destructive move from |other| to this.
  void MoveFrom(ScopedFile& other);

  base::FilePath path_;
  ScopeOutPolicy scope_out_policy_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  ScopeOutCallbackList scope_out_callbacks_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_COMMON_BLOB_SCOPED_FILE_H_
