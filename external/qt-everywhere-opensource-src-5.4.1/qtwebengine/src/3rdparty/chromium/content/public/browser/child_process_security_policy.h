// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace content {

// The ChildProcessSecurityPolicy class is used to grant and revoke security
// capabilities for child processes.  For example, it restricts whether a child
// process is permitted to load file:// URLs based on whether the process
// has ever been commanded to load file:// URLs by the browser.
//
// ChildProcessSecurityPolicy is a singleton that may be used on any thread.
//
class ChildProcessSecurityPolicy {
 public:
  virtual ~ChildProcessSecurityPolicy() {}

  // There is one global ChildProcessSecurityPolicy object for the entire
  // browser process.  The object returned by this method may be accessed on
  // any thread.
  static CONTENT_EXPORT ChildProcessSecurityPolicy* GetInstance();

  // Web-safe schemes can be requested by any child process.  Once a web-safe
  // scheme has been registered, any child process can request URLs with
  // that scheme.  There is no mechanism for revoking web-safe schemes.
  virtual void RegisterWebSafeScheme(const std::string& scheme) = 0;

  // Returns true iff |scheme| has been registered as a web-safe scheme.
  virtual bool IsWebSafeScheme(const std::string& scheme) = 0;

  // This permission grants only read access to a file.
  // Whenever the user picks a file from a <input type="file"> element, the
  // browser should call this function to grant the child process the capability
  // to upload the file to the web. Grants FILE_PERMISSION_READ_ONLY.
  virtual void GrantReadFile(int child_id, const base::FilePath& file) = 0;

  // This permission grants creation, read, and full write access to a file,
  // including attributes.
  virtual void GrantCreateReadWriteFile(int child_id,
                                        const base::FilePath& file) = 0;

  // This permission grants copy-into permission for |dir|.
  virtual void GrantCopyInto(int child_id, const base::FilePath& dir) = 0;

  // This permission grants delete permission for |dir|.
  virtual void GrantDeleteFrom(int child_id, const base::FilePath& dir) = 0;

  // These methods verify whether or not the child process has been granted
  // permissions perform these functions on |file|.

  // Before servicing a child process's request to upload a file to the web, the
  // browser should call this method to determine whether the process has the
  // capability to upload the requested file.
  virtual bool CanReadFile(int child_id, const base::FilePath& file) = 0;
  virtual bool CanCreateReadWriteFile(int child_id,
                                      const base::FilePath& file) = 0;

  // Grants read access permission to the given isolated file system
  // identified by |filesystem_id|. An isolated file system can be
  // created for a set of native files/directories (like dropped files)
  // using fileapi::IsolatedContext. A child process needs to be granted
  // permission to the file system to access the files in it using
  // file system URL. You do NOT need to give direct permission to
  // individual file paths.
  //
  // Note: files/directories in the same file system share the same
  // permission as far as they are accessed via the file system, i.e.
  // using the file system URL (tip: you can create a new file system
  // to give different permission to part of files).
  virtual void GrantReadFileSystem(int child_id,
                                   const std::string& filesystem_id) = 0;

  // Grants write access permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for GrantReadFileSystem
  // for more details.  You do NOT need to give direct permission to
  // individual file paths.
  //
  // This must be called with a great care as this gives write permission
  // to all files/directories included in the file system.
  virtual void GrantWriteFileSystem(int child_id,
                                    const std::string& filesystem_id) = 0;

  // Grants create file permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for GrantReadFileSystem
  // for more details.  You do NOT need to give direct permission to
  // individual file paths.
  //
  // This must be called with a great care as this gives create permission
  // within all directories included in the file system.
  virtual void GrantCreateFileForFileSystem(
      int child_id,
      const std::string& filesystem_id) = 0;

  // Grants create, read and write access permissions to the given isolated
  // file system identified by |filesystem_id|.  See comments for
  // GrantReadFileSystem for more details.  You do NOT need to give direct
  // permission to individual file paths.
  //
  // This must be called with a great care as this gives create, read and write
  // permissions to all files/directories included in the file system.
  virtual void GrantCreateReadWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) = 0;

  // Grants permission to copy-into filesystem |filesystem_id|. 'copy-into'
  // is used to allow copying files into the destination filesystem without
  // granting more general create and write permissions.
  virtual void GrantCopyIntoFileSystem(int child_id,
                                       const std::string& filesystem_id) = 0;

  // Grants permission to delete from filesystem |filesystem_id|. 'delete-from'
  // is used to allow deleting files into the destination filesystem without
  // granting more general create and write permissions.
  virtual void GrantDeleteFromFileSystem(int child_id,
                                         const std::string& filesystem_id) = 0;

  // Grants the child process the capability to access URLs of the provided
  // scheme.
  virtual void GrantScheme(int child_id, const std::string& scheme) = 0;

  // Returns true if read access has been granted to |filesystem_id|.
  virtual bool CanReadFileSystem(int child_id,
                                 const std::string& filesystem_id) = 0;

  // Returns true if read and write access has been granted to |filesystem_id|.
  virtual bool CanReadWriteFileSystem(int child_id,
                                      const std::string& filesystem_id) = 0;

  // Returns true if copy-into access has been granted to |filesystem_id|.
  virtual bool CanCopyIntoFileSystem(int child_id,
                                     const std::string& filesystem_id) = 0;

  // Returns true if delete-from access has been granted to |filesystem_id|.
  virtual bool CanDeleteFromFileSystem(int child_id,
                                       const std::string& filesystem_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
