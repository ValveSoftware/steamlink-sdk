// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_TEMPORARY_FILE_STREAM_H_
#define CONTENT_BROWSER_LOADER_TEMPORARY_FILE_STREAM_H_

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace net {
class FileStream;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {

typedef base::Callback<void(base::File::Error,
                            scoped_ptr<net::FileStream>,
                            webkit_blob::ShareableFileReference*)>
    CreateTemporaryFileStreamCallback;

// Creates a temporary file and asynchronously calls |callback| with a
// net::FileStream and webkit_blob::ShareableFileReference. The file is deleted
// when the webkit_blob::ShareableFileReference is deleted. Note it is the
// consumer's responsibility to ensure the webkit_blob::ShareableFileReference
// stays in scope until net::FileStream has finished closing the file. On error,
// |callback| is called with an error in the first parameter.
//
// This function may only be called on the IO thread.
//
// TODO(davidben): Juggling the net::FileStream and
// webkit_blob::ShareableFileReference lifetimes is a nuisance. The two should
// be tied together so the consumer need not deal with it.
CONTENT_EXPORT void CreateTemporaryFileStream(
    const CreateTemporaryFileStreamCallback& callback);

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_TEMPORARY_FILE_STREAM_H_
