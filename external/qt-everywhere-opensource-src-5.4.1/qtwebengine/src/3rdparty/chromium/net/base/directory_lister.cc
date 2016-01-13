// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

bool IsDotDot(const base::FilePath& path) {
  return FILE_PATH_LITERAL("..") == path.BaseName().value();
}

// Comparator for sorting lister results. This uses the locale aware filename
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool CompareAlphaDirsFirst(const DirectoryLister::DirectoryListerData& a,
                           const DirectoryLister::DirectoryListerData& b) {
  // Parent directory before all else.
  if (IsDotDot(a.info.GetName()))
    return true;
  if (IsDotDot(b.info.GetName()))
    return false;

  // Directories before regular files.
  bool a_is_directory = a.info.IsDirectory();
  bool b_is_directory = b.info.IsDirectory();
  if (a_is_directory != b_is_directory)
    return a_is_directory;

  return file_util::LocaleAwareCompareFilenames(a.info.GetName(),
                                                b.info.GetName());
}

bool CompareDate(const DirectoryLister::DirectoryListerData& a,
                 const DirectoryLister::DirectoryListerData& b) {
  // Parent directory before all else.
  if (IsDotDot(a.info.GetName()))
    return true;
  if (IsDotDot(b.info.GetName()))
    return false;

  // Directories before regular files.
  bool a_is_directory = a.info.IsDirectory();
  bool b_is_directory = b.info.IsDirectory();
  if (a_is_directory != b_is_directory)
    return a_is_directory;
  return a.info.GetLastModifiedTime() > b.info.GetLastModifiedTime();
}

// Comparator for sorting find result by paths. This uses the locale-aware
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool CompareFullPath(const DirectoryLister::DirectoryListerData& a,
                     const DirectoryLister::DirectoryListerData& b) {
  return file_util::LocaleAwareCompareFilenames(a.path, b.path);
}

void SortData(std::vector<DirectoryLister::DirectoryListerData>* data,
              DirectoryLister::SortType sort_type) {
  // Sort the results. See the TODO below (this sort should be removed and we
  // should do it from JS).
  if (sort_type == DirectoryLister::DATE)
    std::sort(data->begin(), data->end(), CompareDate);
  else if (sort_type == DirectoryLister::FULL_PATH)
    std::sort(data->begin(), data->end(), CompareFullPath);
  else if (sort_type == DirectoryLister::ALPHA_DIRS_FIRST)
    std::sort(data->begin(), data->end(), CompareAlphaDirsFirst);
  else
    DCHECK_EQ(DirectoryLister::NO_SORT, sort_type);
}

}  // namespace

DirectoryLister::DirectoryLister(const base::FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : core_(new Core(dir, false, ALPHA_DIRS_FIRST, this)),
      delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!dir.value().empty());
}

DirectoryLister::DirectoryLister(const base::FilePath& dir,
                                 bool recursive,
                                 SortType sort,
                                 DirectoryListerDelegate* delegate)
    : core_(new Core(dir, recursive, sort, this)),
      delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!dir.value().empty());
}

DirectoryLister::~DirectoryLister() {
  Cancel();
}

bool DirectoryLister::Start() {
  return core_->Start();
}

void DirectoryLister::Cancel() {
  return core_->Cancel();
}

DirectoryLister::Core::Core(const base::FilePath& dir,
                            bool recursive,
                            SortType sort,
                            DirectoryLister* lister)
    : dir_(dir),
      recursive_(recursive),
      sort_(sort),
      lister_(lister) {
  DCHECK(lister_);
}

DirectoryLister::Core::~Core() {}

bool DirectoryLister::Core::Start() {
  origin_loop_ = base::MessageLoopProxy::current();

  return base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&Core::StartInternal, this), true);
}

void DirectoryLister::Core::Cancel() {
  lister_ = NULL;
}

void DirectoryLister::Core::StartInternal() {

  if (!base::DirectoryExists(dir_)) {
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DirectoryLister::Core::OnDone, this, ERR_FILE_NOT_FOUND));
    return;
  }

  int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES;
  if (!recursive_)
    types |= base::FileEnumerator::INCLUDE_DOT_DOT;

  base::FileEnumerator file_enum(dir_, recursive_, types);

  base::FilePath path;
  std::vector<DirectoryListerData> file_data;
  while (lister_ && !(path = file_enum.Next()).empty()) {
    DirectoryListerData data;
    data.info = file_enum.GetInfo();
    data.path = path;
    file_data.push_back(data);

    /* TODO(brettw) bug 24107: It would be nice to send incremental updates.
       We gather them all so they can be sorted, but eventually the sorting
       should be done from JS to give more flexibility in the page. When we do
       that, we can uncomment this to send incremental updates to the page.

    const int kFilesPerEvent = 8;
    if (file_data.size() < kFilesPerEvent)
      continue;

    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DirectoryLister::Core::SendData, file_data));
    file_data.clear();
    */
  }

  SortData(&file_data, sort_);
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DirectoryLister::Core::SendData, this, file_data));

  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DirectoryLister::Core::OnDone, this, OK));
}

void DirectoryLister::Core::SendData(
    const std::vector<DirectoryLister::DirectoryListerData>& data) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  // We need to check for cancellation (indicated by NULL'ing of |lister_|)
  // which can happen during each callback.
  for (size_t i = 0; lister_ && i < data.size(); ++i)
    lister_->OnReceivedData(data[i]);
}

void DirectoryLister::Core::OnDone(int error) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  if (lister_)
    lister_->OnDone(error);
}

void DirectoryLister::OnReceivedData(const DirectoryListerData& data) {
  delegate_->OnListFile(data);
}

void DirectoryLister::OnDone(int error) {
  delegate_->OnListDone(error);
}

}  // namespace net
