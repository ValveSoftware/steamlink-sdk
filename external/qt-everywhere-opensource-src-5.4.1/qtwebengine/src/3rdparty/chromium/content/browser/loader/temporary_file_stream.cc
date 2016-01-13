// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/temporary_file_stream.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_proxy.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "webkit/common/blob/shareable_file_reference.h"

using webkit_blob::ShareableFileReference;

namespace content {

namespace {

void DidCreateTemporaryFile(
    const CreateTemporaryFileStreamCallback& callback,
    scoped_ptr<base::FileProxy> file_proxy,
    base::File::Error error_code,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!file_proxy->IsValid()) {
    callback.Run(error_code, scoped_ptr<net::FileStream>(), NULL);
    return;
  }

  scoped_refptr<base::TaskRunner> task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);

  // Cancelled or not, create the deletable_file so the temporary is cleaned up.
  scoped_refptr<ShareableFileReference> deletable_file =
      ShareableFileReference::GetOrCreate(
          file_path,
          ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          task_runner.get());

  scoped_ptr<net::FileStream> file_stream(
      new net::FileStream(file_proxy->TakeFile(), task_runner));

  callback.Run(error_code, file_stream.Pass(), deletable_file);
}

}  // namespace

void CreateTemporaryFileStream(
    const CreateTemporaryFileStreamCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_ptr<base::FileProxy> file_proxy(new base::FileProxy(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE).get()));
  base::FileProxy* proxy = file_proxy.get();
  proxy->CreateTemporary(
      base::File::FLAG_ASYNC,
      base::Bind(&DidCreateTemporaryFile, callback, Passed(&file_proxy)));
}

}  // namespace content
