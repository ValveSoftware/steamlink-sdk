// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/browser_file_system_helper.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "url/url_constants.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_permission_policy.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

namespace {

using fileapi::FileSystemOptions;

FileSystemOptions CreateBrowserFileSystemOptions(bool is_incognito) {
  FileSystemOptions::ProfileMode profile_mode =
      is_incognito ? FileSystemOptions::PROFILE_MODE_INCOGNITO
                   : FileSystemOptions::PROFILE_MODE_NORMAL;
  std::vector<std::string> additional_allowed_schemes;
  GetContentClient()->browser()->GetAdditionalAllowedSchemesForFileSystem(
      &additional_allowed_schemes);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles)) {
    additional_allowed_schemes.push_back(url::kFileScheme);
  }
  return FileSystemOptions(profile_mode, additional_allowed_schemes, NULL);
}

}  // namespace

scoped_refptr<fileapi::FileSystemContext> CreateFileSystemContext(
    BrowserContext* browser_context,
    const base::FilePath& profile_path,
    bool is_incognito,
    quota::QuotaManagerProxy* quota_manager_proxy) {

  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> file_task_runner =
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          pool->GetNamedSequenceToken("FileAPI"),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);

  // Setting up additional filesystem backends.
  ScopedVector<fileapi::FileSystemBackend> additional_backends;
  GetContentClient()->browser()->GetAdditionalFileSystemBackends(
      browser_context,
      profile_path,
      &additional_backends);

  // Set up the auto mount handlers for url requests.
  std::vector<fileapi::URLRequestAutoMountHandler>
      url_request_auto_mount_handlers;
  GetContentClient()->browser()->GetURLRequestAutoMountHandlers(
      &url_request_auto_mount_handlers);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      new fileapi::FileSystemContext(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get(),
          file_task_runner.get(),
          BrowserContext::GetMountPoints(browser_context),
          browser_context->GetSpecialStoragePolicy(),
          quota_manager_proxy,
          additional_backends.Pass(),
          url_request_auto_mount_handlers,
          profile_path,
          CreateBrowserFileSystemOptions(is_incognito));

  std::vector<fileapi::FileSystemType> types;
  file_system_context->GetFileSystemTypes(&types);
  for (size_t i = 0; i < types.size(); ++i) {
    ChildProcessSecurityPolicyImpl::GetInstance()->
        RegisterFileSystemPermissionPolicy(
            types[i],
            fileapi::FileSystemContext::GetPermissionPolicy(types[i]));
  }

  return file_system_context;
}

bool FileSystemURLIsValid(
    fileapi::FileSystemContext* context,
    const fileapi::FileSystemURL& url) {
  if (!url.is_valid())
    return false;

  return context->GetFileSystemBackend(url.type()) != NULL;
}

void SyncGetPlatformPath(fileapi::FileSystemContext* context,
                         int process_id,
                         const GURL& path,
                         base::FilePath* platform_path) {
  DCHECK(context->default_file_task_runner()->
         RunsTasksOnCurrentThread());
  DCHECK(platform_path);
  *platform_path = base::FilePath();
  fileapi::FileSystemURL url(context->CrackURL(path));
  if (!FileSystemURLIsValid(context, url))
    return;

  // Make sure if this file is ok to be read (in the current architecture
  // which means roughly same as the renderer is allowed to get the platform
  // path to the file).
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadFileSystemFile(process_id, url))
    return;

  context->operation_runner()->SyncGetPlatformPath(url, platform_path);

  // The path is to be attached to URLLoader so we grant read permission
  // for the file. (We need to check first because a parent directory may
  // already have the permissions and we don't need to grant it to the file.)
  if (!policy->CanReadFile(process_id, *platform_path))
    policy->GrantReadFile(process_id, *platform_path);
}

}  // namespace content
