// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/safe_json_parser_impl.h"

#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_json {

SafeJsonParserImpl::SafeJsonParserImpl(const std::string& unsafe_json,
                                       const SuccessCallback& success_callback,
                                       const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  io_thread_checker_.DetachFromThread();
}

SafeJsonParserImpl::~SafeJsonParserImpl() = default;

void SafeJsonParserImpl::Start() {
  caller_task_runner_ = base::SequencedTaskRunnerHandle::Get();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeJsonParserImpl::StartOnIOThread, base::Unretained(this)));
}

void SafeJsonParserImpl::StartOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(io_thread_checker_.CalledOnValidThread());
  mojo_json_parser_.reset(
      new content::UtilityProcessMojoClient<mojom::SafeJsonParser>(
          l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_JSON_PARSER_NAME)));

  mojo_json_parser_->set_error_callback(base::Bind(
      &SafeJsonParserImpl::OnConnectionError, base::Unretained(this)));
  mojo_json_parser_->Start();

  mojo_json_parser_->service()->Parse(
      std::move(unsafe_json_),
      base::Bind(&SafeJsonParserImpl::OnParseDone, base::Unretained(this)));
}

void SafeJsonParserImpl::OnConnectionError() {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Shut down the utility process.
  mojo_json_parser_.reset();

  caller_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SafeJsonParserImpl::ReportResults,
                            base::Unretained(this), nullptr, "Json error"));
}

void SafeJsonParserImpl::OnParseDone(const base::ListValue& wrapper,
                                     mojo::String error) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Shut down the utility process.
  mojo_json_parser_.reset();

  std::unique_ptr<base::Value> parsed_json;
  std::string error_message;
  if (!wrapper.empty()) {
    const base::Value* value = nullptr;
    CHECK(wrapper.Get(0, &value));
    parsed_json.reset(value->DeepCopy());
  } else {
    error_message = error.get();
  }

  // Call ReportResults() on caller's thread.
  caller_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SafeJsonParserImpl::ReportResults, base::Unretained(this),
                 base::Passed(&parsed_json), error_message));
}

void SafeJsonParserImpl::ReportResults(std::unique_ptr<base::Value> parsed_json,
                                       const std::string& error) {
  DCHECK(caller_task_runner_->RunsTasksOnCurrentThread());
  if (error.empty() && parsed_json) {
    if (!success_callback_.is_null())
      success_callback_.Run(std::move(parsed_json));
  } else {
    if (!error_callback_.is_null())
      error_callback_.Run(error);
  }

  // The parsing is done whether an error occured or not, so this instance can
  // be cleaned up.
  delete this;
}

}  // namespace safe_json
