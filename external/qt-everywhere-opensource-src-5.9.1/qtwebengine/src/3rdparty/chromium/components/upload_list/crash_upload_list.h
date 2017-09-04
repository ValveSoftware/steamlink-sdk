// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPLOAD_LIST_CRASH_UPLOAD_LIST_H_
#define COMPONENTS_UPLOAD_LIST_CRASH_UPLOAD_LIST_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "components/upload_list/upload_list.h"

namespace base {
class FilePath;
}

// An upload list manager for crash reports from breakpad.
class CrashUploadList : public UploadList {
 public:
  // Should match kReporterLogFilename in
  // breakpad/src/client/apple/Framework/BreakpadDefines.h.
  static const char kReporterLogFilename[];

  // Creates a new crash upload list with the given callback delegate.
  CrashUploadList(Delegate* delegate,
                  const base::FilePath& upload_log_path,
                  scoped_refptr<base::TaskRunner> task_runner);

 protected:
  ~CrashUploadList() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashUploadList);
};

#endif  // COMPONENTS_UPLOAD_LIST_CRASH_UPLOAD_LIST_H_
