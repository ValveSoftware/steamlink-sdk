// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
#define CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/resource_type.h"
#include "storage/common/fileapi/file_system_types.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace content {

class CONTENT_EXPORT ChildProcessSecurityPolicyImpl
    : NON_EXPORTED_BASE(public ChildProcessSecurityPolicy) {
 public:
  // Object can only be created through GetInstance() so the constructor is
  // private.
  ~ChildProcessSecurityPolicyImpl() override;

  static ChildProcessSecurityPolicyImpl* GetInstance();

  // ChildProcessSecurityPolicy implementation.
  void RegisterWebSafeScheme(const std::string& scheme) override;
  void RegisterWebSafeIsolatedScheme(
      const std::string& scheme,
      bool always_allow_in_origin_headers) override;
  bool IsWebSafeScheme(const std::string& scheme) override;
  void GrantReadFile(int child_id, const base::FilePath& file) override;
  void GrantCreateReadWriteFile(int child_id,
                                const base::FilePath& file) override;
  void GrantCopyInto(int child_id, const base::FilePath& dir) override;
  void GrantDeleteFrom(int child_id, const base::FilePath& dir) override;
  void GrantReadFileSystem(int child_id,
                           const std::string& filesystem_id) override;
  void GrantWriteFileSystem(int child_id,
                            const std::string& filesystem_id) override;
  void GrantCreateFileForFileSystem(int child_id,
                                    const std::string& filesystem_id) override;
  void GrantCreateReadWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) override;
  void GrantCopyIntoFileSystem(int child_id,
                               const std::string& filesystem_id) override;
  void GrantDeleteFromFileSystem(int child_id,
                                 const std::string& filesystem_id) override;
  void GrantOrigin(int child_id, const url::Origin& origin) override;
  void GrantScheme(int child_id, const std::string& scheme) override;
  bool CanRequestURL(int child_id, const GURL& url) override;
  bool CanCommitURL(int child_id, const GURL& url) override;
  bool CanReadFile(int child_id, const base::FilePath& file) override;
  bool CanCreateReadWriteFile(int child_id,
                              const base::FilePath& file) override;
  bool CanReadFileSystem(int child_id,
                         const std::string& filesystem_id) override;
  bool CanReadWriteFileSystem(int child_id,
                              const std::string& filesystem_id) override;
  bool CanCopyIntoFileSystem(int child_id,
                             const std::string& filesystem_id) override;
  bool CanDeleteFromFileSystem(int child_id,
                               const std::string& filesystem_id) override;
  bool HasWebUIBindings(int child_id) override;
  void GrantSendMidiSysExMessage(int child_id) override;
  bool CanAccessDataForOrigin(int child_id, const GURL& url) override;
  bool HasSpecificPermissionForOrigin(int child_id,
                                      const url::Origin& origin) override;

  // Returns if |child_id| can read all of the |files|.
  bool CanReadAllFiles(int child_id, const std::vector<base::FilePath>& files);

  // Pseudo schemes are treated differently than other schemes because they
  // cannot be requested like normal URLs.  There is no mechanism for revoking
  // pseudo schemes.
  void RegisterPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as pseudo scheme.
  bool IsPseudoScheme(const std::string& scheme);

  // Upon creation, child processes should register themselves by calling this
  // this method exactly once.
  void Add(int child_id);

  // Upon creation, worker thread child processes should register themselves by
  // calling this this method exactly once. Workers that are not shared will
  // inherit permissions from their parent renderer process identified with
  // |main_render_process_id|.
  void AddWorker(int worker_child_id, int main_render_process_id);

  // Upon destruction, child processess should unregister themselves by caling
  // this method exactly once.
  void Remove(int child_id);

  // Whenever the browser processes commands the child process to request a URL,
  // it should call this method to grant the child process the capability to
  // request the URL, along with permission to request all URLs of the same
  // scheme.
  void GrantRequestURL(int child_id, const GURL& url);

  // Whenever the browser process drops a file icon on a tab, it should call
  // this method to grant the child process the capability to request this one
  // file:// URL, but not all urls of the file:// scheme.
  void GrantRequestSpecificFileURL(int child_id, const GURL& url);

  // Revokes all permissions granted to the given file.
  void RevokeAllPermissionsForFile(int child_id, const base::FilePath& file);

  // Grant the child process the ability to use Web UI Bindings.
  void GrantWebUIBindings(int child_id);

  // Grant the child process the ability to read raw cookies.
  void GrantReadRawCookies(int child_id);

  // Revoke read raw cookies permission.
  void RevokeReadRawCookies(int child_id);

  // Whether the given origin is valid for an origin header. Valid origin
  // headers are commitable URLs plus suborigin URLs.
  bool CanSetAsOriginHeader(int child_id, const GURL& url);

  // Explicit permissions checks for FileSystemURL specified files.
  bool CanReadFileSystemFile(int child_id,
                             const storage::FileSystemURL& filesystem_url);
  bool CanWriteFileSystemFile(int child_id,
                              const storage::FileSystemURL& filesystem_url);
  bool CanCreateFileSystemFile(int child_id,
                               const storage::FileSystemURL& filesystem_url);
  bool CanCreateReadWriteFileSystemFile(
      int child_id,
      const storage::FileSystemURL& filesystem_url);
  bool CanCopyIntoFileSystemFile(int child_id,
                                 const storage::FileSystemURL& filesystem_url);
  bool CanDeleteFileSystemFile(int child_id,
                               const storage::FileSystemURL& filesystem_url);

  // Returns true if the specified child_id has been granted ReadRawCookies.
  bool CanReadRawCookies(int child_id);

  // Sets the process as only permitted to use and see the cookies for the
  // given origin.
  // Origin lock is applied only if the --site-per-process flag is used.
  void LockToOrigin(int child_id, const GURL& gurl);

  // Register FileSystem type and permission policy which should be used
  // for the type.  The |policy| must be a bitwise-or'd value of
  // storage::FilePermissionPolicy.
  void RegisterFileSystemPermissionPolicy(storage::FileSystemType type,
                                          int policy);

  // Returns true if sending system exclusive messages is allowed.
  bool CanSendMidiSysExMessage(int child_id);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  friend class ChildProcessSecurityPolicyTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, FilePermissions);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, std::unique_ptr<SecurityState>> SecurityStateMap;
  typedef std::map<int, int> WorkerToMainProcessMap;
  typedef std::map<storage::FileSystemType, int> FileSystemPermissionPolicyMap;

  // Obtain an instance of ChildProcessSecurityPolicyImpl via GetInstance().
  ChildProcessSecurityPolicyImpl();
  friend struct base::DefaultSingletonTraits<ChildProcessSecurityPolicyImpl>;

  // Adds child process during registration.
  void AddChild(int child_id);

  // Determines if certain permissions were granted for a file to given child
  // process. |permissions| is an internally defined bit-set.
  bool ChildProcessHasPermissionsForFile(int child_id,
                                         const base::FilePath& file,
                                         int permissions);

  // Grant a particular permission set for a file. |permissions| is an
  // internally defined bit-set.
  void GrantPermissionsForFile(int child_id,
                               const base::FilePath& file,
                               int permissions);

  // Grants access permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for
  // ChildProcessSecurityPolicy::GrantReadFileSystem() for more details.
  void GrantPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

  // Determines if certain permissions were granted for a file. |permissions|
  // is an internally defined bit-set. If |child_id| is a worker process,
  // this returns true if either the worker process or its parent renderer
  // has permissions for the file.
  bool HasPermissionsForFile(int child_id,
                             const base::FilePath& file,
                             int permissions);

  // Determines if certain permissions were granted for a file in FileSystem
  // API. |permissions| is an internally defined bit-set.
  bool HasPermissionsForFileSystemFile(
      int child_id,
      const storage::FileSystemURL& filesystem_url,
      int permissions);

  // Determines if certain permissions were granted for a file system.
  // |permissions| is an internally defined bit-set.
  bool HasPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  base::Lock lock_;

  // These schemes are white-listed for all child processes in various contexts.
  // These sets are protected by |lock_|.
  SchemeSet schemes_okay_to_commit_in_any_process_;
  SchemeSet schemes_okay_to_request_in_any_process_;
  SchemeSet schemes_okay_to_appear_as_origin_headers_;

  // These schemes do not actually represent retrievable URLs.  For example,
  // the the URLs in the "about" scheme are aliases to other URLs.  This set is
  // protected by |lock_|.
  SchemeSet pseudo_schemes_;

  // This map holds a SecurityState for each child process.  The key for the
  // map is the ID of the ChildProcessHost.  The SecurityState objects are
  // owned by this object and are protected by |lock_|.  References to them must
  // not escape this class.
  SecurityStateMap security_state_;

  // This maps keeps the record of which js worker thread child process
  // corresponds to which main js thread child process.
  WorkerToMainProcessMap worker_map_;

  FileSystemPermissionPolicyMap file_system_policy_map_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
