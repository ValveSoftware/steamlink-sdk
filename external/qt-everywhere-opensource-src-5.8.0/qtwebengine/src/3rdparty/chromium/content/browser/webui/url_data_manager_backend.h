// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
#define CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/url_data_source.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace content {

class ChromeBlobStorageContext;
class ResourceContext;
class URLDataManagerBackend;
class URLDataSourceImpl;
class URLRequestChromeJob;

// URLDataManagerBackend is used internally by ChromeURLDataManager on the IO
// thread. In most cases you can use the API in ChromeURLDataManager and ignore
// this class. URLDataManagerBackend is owned by ResourceContext.
class URLDataManagerBackend : public base::SupportsUserData::Data {
 public:
  typedef int RequestID;

  URLDataManagerBackend();
  ~URLDataManagerBackend() override;

  // Invoked to create the protocol handler for chrome://. |is_incognito| should
  // be set for incognito profiles. Called on the UI thread.
  CONTENT_EXPORT static std::unique_ptr<
      net::URLRequestJobFactory::ProtocolHandler>
  CreateProtocolHandler(content::ResourceContext* resource_context,
                        bool is_incognito,
                        ChromeBlobStorageContext* blob_storage_context);

  // Adds a DataSource to the collection of data sources.
  void AddDataSource(URLDataSourceImpl* source);

  // DataSource invokes this. Sends the data to the URLRequest.
  void DataAvailable(RequestID request_id, base::RefCountedMemory* bytes);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  friend class URLRequestChromeJob;

  typedef std::map<std::string,
      scoped_refptr<URLDataSourceImpl> > DataSourceMap;
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;

  // Called by the job when it's starting up.
  // Returns false if |url| is not a URL managed by this object.
  bool StartRequest(const net::URLRequest* request, URLRequestChromeJob* job);

  // Helper function to call StartDataRequest on |source|'s delegate. This is
  // needed because while we want to call URLDataSourceDelegate's method, we
  // need to add a refcount on the source.
  static void CallStartRequest(scoped_refptr<URLDataSourceImpl> source,
                               const std::string& path,
                               int render_process_id,
                               int render_frame_id,
                               int request_id);

  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Returns true if the job exists in |pending_requests_|. False otherwise.
  // Called by ~URLRequestChromeJob to verify that |pending_requests_| is kept
  // up to date.
  bool HasPendingJob(URLRequestChromeJob* job) const;

  // Look up the data source for the request. Returns the source if it is found,
  // else NULL.
  URLDataSourceImpl* GetDataSourceFromURL(const GURL& url);

  // Custom sources of data, keyed by source path (e.g. "favicon").
  DataSourceMap data_sources_;

  // All pending URLRequestChromeJobs, keyed by ID of the request.
  // URLRequestChromeJob calls into this object when it's constructed and
  // destructed to ensure that the pointers in this map remain valid.
  PendingRequestMap pending_requests_;

  // The ID we'll use for the next request we receive.
  RequestID next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(URLDataManagerBackend);
};

// Creates protocol handler for chrome-devtools://. |is_incognito| should be
// set for incognito profiles.
net::URLRequestJobFactory::ProtocolHandler*
CreateDevToolsProtocolHandler(content::ResourceContext* resource_context,
                              bool is_incognito);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
