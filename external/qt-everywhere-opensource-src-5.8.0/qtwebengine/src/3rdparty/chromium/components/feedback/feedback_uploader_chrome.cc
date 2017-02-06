// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_chrome.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"
#include "components/feedback/feedback_uploader_delegate.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace feedback {
namespace {

const char kProtoBufMimeType[] = "application/x-protobuf";

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(
    content::BrowserContext* context)
    : FeedbackUploader(context ? context->GetPath() : base::FilePath(),
                       BrowserThread::GetBlockingPool()),
      context_(context) {
  CHECK(context_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kFeedbackServer))
    url_ = command_line.GetSwitchValueASCII(switches::kFeedbackServer);
}

void FeedbackUploaderChrome::DispatchReport(const std::string& data) {
  GURL post_url(url_);

  // Note: FeedbackUploaderDelegate deletes itself and the fetcher.
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(
          post_url, net::URLFetcher::POST,
          new FeedbackUploaderDelegate(
              data, base::Bind(&FeedbackUploaderChrome::UpdateUploadTimer,
                               AsWeakPtr()),
              base::Bind(&FeedbackUploaderChrome::RetryReport, AsWeakPtr())))
          .release();

  // Tell feedback server about the variation state of this install.
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(
      fetcher->GetOriginalURL(), context_->IsOffTheRecord(), false, &headers);
  fetcher->SetExtraRequestHeaders(headers.ToString());

  fetcher->SetUploadData(kProtoBufMimeType, data);
  fetcher->SetRequestContext(
      content::BrowserContext::GetDefaultStoragePartition(context_)->
          GetURLRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->Start();
}

}  // namespace feedback
