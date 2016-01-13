// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webfileutilities_impl.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/child/blink_glue.h"
#include "net/base/file_stream.h"
#include "net/base/filename_util.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebString;

namespace content {

WebFileUtilitiesImpl::WebFileUtilitiesImpl()
    : sandbox_enabled_(true) {
}

WebFileUtilitiesImpl::~WebFileUtilitiesImpl() {
}

bool WebFileUtilitiesImpl::getFileInfo(const WebString& path,
                                       blink::WebFileInfo& web_file_info) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return false;
  }
  base::File::Info file_info;
  if (!base::GetFileInfo(base::FilePath::FromUTF16Unsafe(path),
                         reinterpret_cast<base::File::Info*>(&file_info)))
    return false;

  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

WebString WebFileUtilitiesImpl::directoryName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).DirName().AsUTF16Unsafe();
}

WebString WebFileUtilitiesImpl::baseName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).BaseName().AsUTF16Unsafe();
}

blink::WebURL WebFileUtilitiesImpl::filePathToURL(const WebString& path) {
  return net::FilePathToFileURL(base::FilePath::FromUTF16Unsafe(path));
}

}  // namespace content
