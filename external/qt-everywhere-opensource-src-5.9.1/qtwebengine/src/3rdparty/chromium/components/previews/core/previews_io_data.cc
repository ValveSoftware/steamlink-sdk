// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_io_data.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/previews/core/previews_ui_service.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace previews {

namespace {

void LogPreviewsEligibilityReason(PreviewsEligibilityReason status,
                                  PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.EligibilityReason.Offline", static_cast<int>(status),
          static_cast<int>(PreviewsEligibilityReason::LAST));
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

PreviewsIOData::PreviewsIOData(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      weak_factory_(this) {}

PreviewsIOData::~PreviewsIOData() {}

void PreviewsIOData::Initialize(
    base::WeakPtr<PreviewsUIService> previews_ui_service,
    std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  previews_ui_service_ = previews_ui_service;

  // Set up the IO thread portion of |this|.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PreviewsIOData::InitializeOnIOThread, base::Unretained(this),
                 base::Passed(&previews_opt_out_store)));
}

void PreviewsIOData::InitializeOnIOThread(
    std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  previews_black_list_.reset(
      new PreviewsBlackList(std::move(previews_opt_out_store),
                            base::MakeUnique<base::DefaultClock>()));
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsUIService::SetIOData, previews_ui_service_,
                            weak_factory_.GetWeakPtr()));
}

void PreviewsIOData::AddPreviewNavigation(const GURL& url,
                                          bool opt_out,
                                          PreviewsType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  previews_black_list_->AddPreviewNavigation(url, opt_out, type);
}

void PreviewsIOData::ClearBlackList(base::Time begin_time,
                                    base::Time end_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  previews_black_list_->ClearBlackList(begin_time, end_time);
}

bool PreviewsIOData::ShouldAllowPreview(const net::URLRequest& request,
                                        PreviewsType type) const {
  if (!IsPreviewsTypeEnabled(type))
    return false;
  // The blacklist will disallow certain hosts for periods of time based on
  // user's opting out of the preview
  if (!previews_black_list_) {
    LogPreviewsEligibilityReason(
        PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE, type);
    return false;
  }
  PreviewsEligibilityReason status =
      previews_black_list_->IsLoadedAndAllowed(request.url(), type);
  if (status != PreviewsEligibilityReason::ALLOWED) {
    LogPreviewsEligibilityReason(status, type);
    return false;
  }
  net::NetworkQualityEstimator* network_quality_estimator =
      request.context()->network_quality_estimator();
  if (!network_quality_estimator ||
      network_quality_estimator->GetEffectiveConnectionType() <
          net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    LogPreviewsEligibilityReason(
        PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE, type);
    return false;
  }
  if (network_quality_estimator->GetEffectiveConnectionType() >
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G) {
    LogPreviewsEligibilityReason(PreviewsEligibilityReason::NETWORK_NOT_SLOW,
                                 type);
    return false;
  }
  LogPreviewsEligibilityReason(PreviewsEligibilityReason::ALLOWED, type);
  return true;
}

}  // namespace previews
