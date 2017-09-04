// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/font_data_fetcher_impl.h"

#include <memory>

#include "base/task_runner_util.h"

namespace blimp {
namespace engine {

namespace {
// This is a temporary implementation which return a empty SkStream.
std::unique_ptr<SkStream>
FetchFontStreamOnFileThread(const std::string& font_hash) {
  return base::MakeUnique<SkMemoryStream>();
}

}  // namespace

FontDataFetcherImpl::FontDataFetcherImpl(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner) {
  DCHECK(file_task_runner_);
}

FontDataFetcherImpl::~FontDataFetcherImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FontDataFetcherImpl::FetchFontStream(
    const std::string& font_hash, const FontResponseCallback& callback) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&FetchFontStreamOnFileThread, font_hash), callback);
}

}  // namespace engine
}  // namespace blimp
