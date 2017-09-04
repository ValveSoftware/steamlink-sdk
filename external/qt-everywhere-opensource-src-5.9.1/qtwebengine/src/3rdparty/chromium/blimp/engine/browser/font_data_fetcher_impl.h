// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_IMPL_H_
#define BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_IMPL_H_

#include <string>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "blimp/engine/browser/font_data_fetcher.h"

namespace blimp {
namespace engine {

// Implementation of fetching font data from files. File access is done on the
// file thread.
class FontDataFetcherImpl : public FontDataFetcher {
 public:
  // |file_task_runner|: The task runner that will process all file access.
  explicit FontDataFetcherImpl(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

  ~FontDataFetcherImpl() override;

  // FontDataFetcher implementation.
  void FetchFontStream(const std::string& font_hash,
                       const FontResponseCallback& callback) const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FontDataFetcherImpl);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_IMPL_H_
