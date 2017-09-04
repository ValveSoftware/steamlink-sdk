// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_ui_service.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "components/previews/core/previews_io_data.h"
#include "url/gurl.h"

namespace previews {

PreviewsUIService::PreviewsUIService(
    PreviewsIOData* previews_io_data,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store)
    : io_task_runner_(io_task_runner), weak_factory_(this) {
  previews_io_data->Initialize(weak_factory_.GetWeakPtr(),
                               std::move(previews_opt_out_store));
}

PreviewsUIService::~PreviewsUIService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PreviewsUIService::SetIOData(base::WeakPtr<PreviewsIOData> io_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_data_ = io_data;
}

void PreviewsUIService::AddPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsIOData::AddPreviewNavigation, io_data_,
                            url, opt_out, type));
}

void PreviewsUIService::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsIOData::ClearBlackList, io_data_,
                            begin_time, end_time));
}

}  // namespace previews
