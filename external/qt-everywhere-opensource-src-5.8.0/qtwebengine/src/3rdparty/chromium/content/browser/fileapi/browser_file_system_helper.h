// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
#define CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace storage {
class ExternalMountPoints;
class FileSystemContext;
class FileSystemURL;
}

namespace content {

class BrowserContext;

// Helper method that returns FileSystemContext constructed for
// the browser process.
CONTENT_EXPORT scoped_refptr<storage::FileSystemContext>
    CreateFileSystemContext(BrowserContext* browser_context,
                            const base::FilePath& profile_path,
                            bool is_incognito,
                            storage::QuotaManagerProxy* quota_manager_proxy);

// Verifies that |url| is valid and has a registered backend in |context|.
CONTENT_EXPORT bool FileSystemURLIsValid(storage::FileSystemContext* context,
                                         const storage::FileSystemURL& url);

// Get the platform path from a file system URL. This needs to be called
// on the FILE thread.
CONTENT_EXPORT void SyncGetPlatformPath(storage::FileSystemContext* context,
                                        int process_id,
                                        const GURL& path,
                                        base::FilePath* platform_path);
}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
