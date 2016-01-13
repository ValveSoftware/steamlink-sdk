// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tcmalloc_internals_request_job.h"

#include "base/allocator/allocator_extension.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "net/base/net_errors.h"

namespace content {

// static
AboutTcmallocOutputs* AboutTcmallocOutputs::GetInstance() {
  return Singleton<AboutTcmallocOutputs>::get();
}

AboutTcmallocOutputs::AboutTcmallocOutputs() {}

AboutTcmallocOutputs::~AboutTcmallocOutputs() {}

void AboutTcmallocOutputs::OnStatsForChildProcess(
    base::ProcessId pid, int process_type,
    const std::string& output) {
  std::string header = GetProcessTypeNameInEnglish(process_type);
  base::StringAppendF(&header, " PID %d", static_cast<int>(pid));
  SetOutput(header, output);
}

void AboutTcmallocOutputs::SetOutput(const std::string& header,
                                     const std::string& output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  outputs_[header] = output;
}

void AboutTcmallocOutputs::DumpToHTMLTable(std::string* data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  data->append("<table width=\"100%\">\n");
  for (AboutTcmallocOutputsType::const_iterator oit = outputs_.begin();
       oit != outputs_.end();
       oit++) {
    data->append("<tr><td bgcolor=\"yellow\">");
    data->append(oit->first);
    data->append("</td></tr>\n");
    data->append("<tr><td><pre>\n");
    data->append(oit->second);
    data->append("</pre></td></tr>\n");
  }
  data->append("</table>\n");
  outputs_.clear();
}

TcmallocInternalsRequestJob::TcmallocInternalsRequestJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate)
    : net::URLRequestSimpleJob(request, network_delegate) {
}

#if defined(USE_TCMALLOC)
void RequestTcmallocStatsFromChildRenderProcesses() {
  RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->Send(new ChildProcessMsg_GetTcmallocStats);
    it.Advance();
  }
}

void AboutTcmalloc(std::string* data) {
  data->append("<!DOCTYPE html>\n<html>\n<head>\n");
  data->append(
      "<meta http-equiv=\"Content-Security-Policy\" "
      "content=\"object-src 'none'; script-src 'none'\">");
  data->append("<title>tcmalloc stats</title>");
  data->append("</head><body>");

  // Display any stats for which we sent off requests the last time.
  data->append("<p>Stats as of last page load;");
  data->append("reload to get stats as of this page load.</p>\n");
  data->append("<table width=\"100%\">\n");

  AboutTcmallocOutputs::GetInstance()->DumpToHTMLTable(data);

  data->append("</body></html>\n");

  // Populate the collector with stats from the local browser process
  // and send off requests to all the renderer processes.
  char buffer[1024 * 32];
  base::allocator::GetStats(buffer, sizeof(buffer));
  std::string browser("Browser");
  AboutTcmallocOutputs::GetInstance()->SetOutput(browser, buffer);

  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    iter.Send(new ChildProcessMsg_GetTcmallocStats);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &RequestTcmallocStatsFromChildRenderProcesses));
}
#endif

int TcmallocInternalsRequestJob::GetData(
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    const net::CompletionCallback& callback) const {
  mime_type->assign("text/html");
  charset->assign("UTF8");

  data->clear();
#if defined(USE_TCMALLOC)
  AboutTcmalloc(data);
#endif
  return net::OK;
}

} // namespace content
