// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_simple.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromecast {
namespace shell {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL("http://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

}  // namespace

CastServiceSimple::CastServiceSimple(
    content::BrowserContext* browser_context,
    PrefService* pref_service)
    : CastService(browser_context, pref_service) {
}

CastServiceSimple::~CastServiceSimple() {
}

void CastServiceSimple::InitializeInternal() {
  startup_url_ = GetStartupURL();
}

void CastServiceSimple::FinalizeInternal() {
}

void CastServiceSimple::StartInternal() {
  window_.reset(new CastContentWindow);
  web_contents_ = window_->CreateWebContents(browser_context());
  window_->CreateWindowTree(web_contents_.get());

  web_contents_->GetController().LoadURL(startup_url_, content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED,
                                         std::string());
}

void CastServiceSimple::StopInternal() {
  web_contents_->ClosePage();
  web_contents_.reset();
  window_.reset();
}

}  // namespace shell
}  // namespace chromecast
