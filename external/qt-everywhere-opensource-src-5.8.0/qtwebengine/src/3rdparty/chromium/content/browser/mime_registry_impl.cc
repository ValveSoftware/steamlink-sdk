// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mime_registry_impl.h"

#include "base/files/file_path.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/common/common_type_converters.h"
#include "net/base/mime_util.h"

namespace content {

// static
void MimeRegistryImpl::Create(blink::mojom::MimeRegistryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  new MimeRegistryImpl(std::move(request));
}

MimeRegistryImpl::MimeRegistryImpl(blink::mojom::MimeRegistryRequest request)
    : binding_(this, std::move(request)) {}

MimeRegistryImpl::~MimeRegistryImpl() = default;

void MimeRegistryImpl::GetMimeTypeFromExtension(
    const mojo::String& extension,
    const GetMimeTypeFromExtensionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::string mime_type;
  net::GetMimeTypeFromExtension(extension.To<base::FilePath::StringType>(),
                                &mime_type);
  callback.Run(mime_type);
}

}  // namespace content
