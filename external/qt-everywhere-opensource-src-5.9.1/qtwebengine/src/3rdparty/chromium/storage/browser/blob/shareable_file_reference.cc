// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/shareable_file_reference.h"

#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/task_runner.h"
#include "base/threading/non_thread_safe.h"

namespace storage {

namespace {

// A shareable file map with enforcement of thread checker.
class ShareableFileMap : public base::NonThreadSafe {
 public:
  typedef std::map<base::FilePath, ShareableFileReference*> FileMap;
  typedef FileMap::iterator iterator;
  typedef FileMap::key_type key_type;
  typedef FileMap::value_type value_type;

  ShareableFileMap() {}

  ~ShareableFileMap() {
    DetachFromThread();
  }

  iterator Find(key_type key) {
    DCHECK(CalledOnValidThread());
    return file_map_.find(key);
  }

  iterator End() {
    DCHECK(CalledOnValidThread());
    return file_map_.end();
  }

  std::pair<iterator, bool> Insert(value_type value) {
    DCHECK(CalledOnValidThread());
    return file_map_.insert(value);
  }

  void Erase(key_type key) {
    DCHECK(CalledOnValidThread());
    file_map_.erase(key);
  }

 private:
  FileMap file_map_;
  DISALLOW_COPY_AND_ASSIGN(ShareableFileMap);
};

base::LazyInstance<ShareableFileMap> g_file_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::Get(
    const base::FilePath& path) {
  ShareableFileMap::iterator found = g_file_map.Get().Find(path);
  ShareableFileReference* reference =
      (found == g_file_map.Get().End()) ? NULL : found->second;
  return scoped_refptr<ShareableFileReference>(reference);
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    const base::FilePath& path,
    FinalReleasePolicy policy,
    base::TaskRunner* file_task_runner) {
  return GetOrCreate(
      ScopedFile(path, static_cast<ScopedFile::ScopeOutPolicy>(policy),
                 file_task_runner));
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    ScopedFile scoped_file) {
  if (scoped_file.path().empty())
    return scoped_refptr<ShareableFileReference>();

  typedef std::pair<ShareableFileMap::iterator, bool> InsertResult;
  // Required for VS2010:
  // http://connect.microsoft.com/VisualStudio/feedback/
  // details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  storage::ShareableFileReference* null_reference = NULL;
  InsertResult result = g_file_map.Get().Insert(
      ShareableFileMap::value_type(scoped_file.path(), null_reference));
  if (result.second == false) {
    scoped_file.Release();
    return scoped_refptr<ShareableFileReference>(result.first->second);
  }

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<ShareableFileReference> reference(
      new ShareableFileReference(std::move(scoped_file)));
  result.first->second = reference.get();
  return reference;
}

void ShareableFileReference::AddFinalReleaseCallback(
    const FinalReleaseCallback& callback) {
  DCHECK(g_file_map.Get().CalledOnValidThread());
  scoped_file_.AddScopeOutCallback(callback, NULL);
}

ShareableFileReference::ShareableFileReference(ScopedFile scoped_file)
    : scoped_file_(std::move(scoped_file)) {
  DCHECK(g_file_map.Get().Find(path())->second == NULL);
}

ShareableFileReference::~ShareableFileReference() {
  DCHECK(g_file_map.Get().Find(path())->second == this);
  g_file_map.Get().Erase(path());
}

}  // namespace storage
