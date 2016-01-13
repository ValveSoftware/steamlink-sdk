// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_
#define WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_

#include <windows.h>
#include <atldef.h>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

namespace base {
class CommandLine;
}

namespace delegate_execute {

// Parses a portion of the DelegateExecute handler's command line to determine
// the desired operation. The operation type is decided by looking at the
// command line. The operations are:
// DELEGATE_EXECUTE:
//   When the delegate_execute.exe is invoked by windows when a chrome
//   activation via the shell, possibly using ShellExecute. Control must
//   be given to ATLs WinMain.
// RELAUNCH_CHROME:
//   When the delegate_execute.exe is launched by chrome, when chrome needs
//   to re-launch itself. The required command line parameters are:
//     --relaunch-shortcut=<PathToShortcut>
//     --wait-for-mutex=<MutexNamePid>
//   The PathToShortcut must be the fully qualified file name to the chrome
//   shortcut that has the appId and other 'metro ready' parameters.
//   The MutexNamePid is a mutex name that also encodes the process id and
//   must follow the format <A>.<B>.<pid> where A and B are arbitray strings
//   (usually chrome.relaunch) and pid is the process id of chrome.
class DelegateExecuteOperation {
 public:
  enum OperationType {
    DELEGATE_EXECUTE,
    RELAUNCH_CHROME,
  };

  DelegateExecuteOperation();
  ~DelegateExecuteOperation();

  bool Init(const base::CommandLine* cmd_line);

  OperationType operation_type() const {
    return operation_type_;
  }

  const base::string16& relaunch_flags() const {
    return relaunch_flags_;
  }

  const base::string16& mutex() const {
    return mutex_;
  }

  // Returns the process id of the parent or 0 on failure.
  DWORD GetParentPid() const;

  const base::FilePath& shortcut() const {
    return relaunch_shortcut_;
  }

 private:
  OperationType operation_type_;
  base::string16 relaunch_flags_;
  base::FilePath relaunch_shortcut_;
  base::string16 mutex_;

  DISALLOW_COPY_AND_ASSIGN(DelegateExecuteOperation);
};

}  // namespace delegate_execute

#endif  // WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_
