// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_internals_request_job.h"

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "content/browser/histogram_synchronizer.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace content {

HistogramInternalsRequestJob::HistogramInternalsRequestJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate)
    : net::URLRequestSimpleJob(request, network_delegate) {
  const std::string& spec = request->url().possibly_invalid_spec();
  const url::Parsed& parsed = request->url().parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path_.assign(spec.substr(offset));
}

void AboutHistogram(std::string* data, const std::string& path) {
  HistogramSynchronizer::FetchHistograms();

  std::string unescaped_query;
  std::string unescaped_title("About Histograms");
  if (!path.empty()) {
    unescaped_query = net::UnescapeURLComponent(path,
                                                net::UnescapeRule::NORMAL);
    unescaped_title += " - " + unescaped_query;
  }

  data->append("<!DOCTYPE html>\n<html>\n<head>\n");
  data->append(
      "<meta http-equiv=\"Content-Security-Policy\" "
      "content=\"object-src 'none'; script-src 'none'\">");
  data->append("<title>");
  data->append(net::EscapeForHTML(unescaped_title));
  data->append("</title>\n");
  data->append("</head><body>");

  // Display any stats for which we sent off requests the last time.
  data->append("<p>Stats as of last page load;");
  data->append("reload to get stats as of this page load.</p>\n");
  data->append("<table width=\"100%\">\n");

  base::StatisticsRecorder::WriteHTMLGraph(unescaped_query, data);
}

int HistogramInternalsRequestJob::GetData(
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    const net::CompletionCallback& callback) const {
  mime_type->assign("text/html");
  charset->assign("UTF8");

  data->clear();
  AboutHistogram(data, path_);
  return net::OK;
}

}  // namespace content
