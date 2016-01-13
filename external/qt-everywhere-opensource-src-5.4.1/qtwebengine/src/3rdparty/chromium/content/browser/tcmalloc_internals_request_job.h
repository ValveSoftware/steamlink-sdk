// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TCMALLOC_INTERNALS_REQUEST_JOB_H_
#define CONTENT_BROWSER_TCMALLOC_INTERNALS_REQUEST_JOB_H_

#include <map>
#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "build/build_config.h"  // USE_TCMALLOC
#include "net/url_request/url_request_simple_job.h"

namespace content {

class AboutTcmallocOutputs {
 public:
  // Returns the singleton instance.
  static AboutTcmallocOutputs* GetInstance();

  // Records the output for a specified header string.
  void SetOutput(const std::string& header, const std::string& output);

  void DumpToHTMLTable(std::string* data);

  // Callback for output returned from a child process.  Adds
  // the output for a canonical process-specific header string that
  // incorporates the pid.
  void OnStatsForChildProcess(base::ProcessId pid,
                              int process_type,
                              const std::string& output);

 private:
  AboutTcmallocOutputs();
  ~AboutTcmallocOutputs();

  // A map of header strings (e.g. "Browser", "Renderer PID 123")
  // to the tcmalloc output collected for each process.
  typedef std::map<std::string, std::string> AboutTcmallocOutputsType;
  AboutTcmallocOutputsType outputs_;

  friend struct DefaultSingletonTraits<AboutTcmallocOutputs>;

  DISALLOW_COPY_AND_ASSIGN(AboutTcmallocOutputs);
};

class TcmallocInternalsRequestJob : public net::URLRequestSimpleJob {
 public:
  TcmallocInternalsRequestJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate);

  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE;

 protected:
  virtual ~TcmallocInternalsRequestJob() {}

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TcmallocInternalsRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TCMALLOC_INTERNALS_REQUEST_JOB_H_
