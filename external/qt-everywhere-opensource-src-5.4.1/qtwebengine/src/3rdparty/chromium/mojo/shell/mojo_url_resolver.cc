// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/mojo_url_resolver.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "net/base/filename_util.h"
#include "url/url_util.h"

namespace mojo {
namespace shell {
namespace {

std::string MakeSharedLibraryName(const std::string& host_name) {
#if defined(OS_WIN)
  return host_name + ".dll";
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  return "lib" + host_name + ".so";
#elif defined(OS_MACOSX)
  return "lib" + host_name + ".dylib";
#else
  NOTREACHED() << "dynamic loading of services not supported";
  return std::string();
#endif
}

}  // namespace

MojoURLResolver::MojoURLResolver() {
  // Needed to treat first component of mojo URLs as host, not path.
  url::AddStandardScheme("mojo");
}

MojoURLResolver::~MojoURLResolver() {
}

void MojoURLResolver::AddCustomMapping(const GURL& mojo_url,
                                       const GURL& resolved_url) {
  url_map_[mojo_url] = resolved_url;
}

void MojoURLResolver::AddLocalFileMapping(const GURL& mojo_url) {
  local_file_set_.insert(mojo_url);
}

GURL MojoURLResolver::Resolve(const GURL& mojo_url) const {
  std::map<GURL, GURL>::const_iterator it = url_map_.find(mojo_url);
  if (it != url_map_.end())
    return it->second;

  std::string lib = MakeSharedLibraryName(mojo_url.host());

  if (local_file_set_.find(mojo_url) != local_file_set_.end()) {
    // Resolve to a local file URL.
    base::FilePath path;
#if defined(OS_ANDROID)
    // On Android, additional lib are bundled.
    PathService::Get(base::DIR_MODULE, &path);
#else
    PathService::Get(base::DIR_EXE, &path);
#if !defined(OS_WIN)
    path = path.Append(FILE_PATH_LITERAL("lib"));
#endif  // !defined(OS_WIN)
#endif  // defined(OS_ANDROID)
    path = path.Append(base::FilePath::FromUTF8Unsafe(lib));
    return net::FilePathToFileURL(path);
  }

  // Otherwise, resolve to an URL relative to origin_.
  return GURL(origin_ + "/" + lib);
}

}  // namespace shell
}  // namespace mojo
